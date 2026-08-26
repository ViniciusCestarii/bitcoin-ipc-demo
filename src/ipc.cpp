#include <cstring>
#include <iostream>
#include <ostream>
#include <sys/socket.h>
#include <sys/un.h>

#include <capnp/dynamic.h>
#include <capnp/pretty-print.h>
#include <capnp/rpc-twoparty.h>
#include <kj/async-io.h>

#include <schemas/capnp/mp/proxy.capnp.h>
#include <schemas/capnp/init.capnp.h>
#include <schemas/capnp/mining.capnp.h>


int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: bitcoin-ipc <path-to-node.sock>\n";
        return 1;
    }

    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un addr{.sun_family = AF_UNIX};
    std::strncpy(addr.sun_path, argv[1], sizeof(addr.sun_path) - 1);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("connect");
        return 1;
    }

    auto io = kj::setupAsyncIo();
    auto stream = io.lowLevelProvider->wrapSocketFd(fd, kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP);
    capnp::TwoPartyClient client(*stream);

    auto init = client.bootstrap().castAs<ipc::capnp::messages::Init>();

    auto constructPromise = init.constructRequest().send();
    auto threadMapClient = constructPromise.getThreadMap();

    auto makePoolReq = threadMapClient.makePoolRequest();
    makePoolReq.setCount(2);
    auto makePoolPromise = makePoolReq.send();

    auto miningClient = init.makeMiningRequest().send().getResult();

    auto blockTemplateRequest = miningClient.createNewBlockRequest();
    blockTemplateRequest.setCooldown(false);
    auto blockTemplate = blockTemplateRequest.send().getResult();

    auto coinbaseRequest = blockTemplate.getCoinbaseTxRequest().send();
    auto feesRequest = blockTemplate.getTxFeesRequest().send();
    auto blockSigopsRequest = blockTemplate.getTxSigopsRequest().send();

    auto coinbase = coinbaseRequest.wait(io.waitScope).getResult();
    auto fees = feesRequest.wait(io.waitScope).getResult();
    auto blockSigops = blockSigopsRequest.wait(io.waitScope).getResult();

    std::cout << "Coinbase Version " << coinbase.getVersion() << std::endl;
    std::cout << "Fees " << fees.size() << std::endl;
    std::cout << "Block sigops " << blockSigops.size() << std::endl;
}
