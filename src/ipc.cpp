#include <cstring>
#include <iostream>
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

    auto collectTxsReq = miningClient.collectTxsRequest();
    const kj::byte wtxidBytes[32] = { // random wtxid
        0xb6, 0xf6, 0x99, 0x1d, 0x03, 0xdf, 0x0e, 0x2e,
        0x04, 0xda, 0xff, 0xfc, 0xd6, 0xbc, 0x41, 0x8a,
        0xac, 0x66, 0x04, 0x9e, 0x2c, 0xfa, 0x1b, 0x8d,
        0x1a, 0x0f, 0x9f, 0xb7, 0x0f, 0x7c, 0x1a, 0x2f,
    };
    capnp::Data::Reader wtxid(wtxidBytes, sizeof(wtxidBytes));
    collectTxsReq.setWtxids({wtxid});
    auto txCollectionClient = collectTxsReq.send().getResult();
    // finally call .wait(), all this is handled in a single round trip
    auto unknownTxPosResult = txCollectionClient.unknownTxPosRequest().send().wait(io.waitScope).getResult();

    std::cout << "Unknown txs count: " << unknownTxPosResult.size() << std::endl;
}
