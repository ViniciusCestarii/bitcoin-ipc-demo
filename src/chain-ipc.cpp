#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>

#include <capnp/rpc-twoparty.h>
#include <kj/async-io.h>

#include <ipc/capnp/mp/proxy.capnp.h>
#include <ipc/capnp/init.capnp.h>
#include <ipc/capnp/chain.capnp.h>

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: chain-ipc <path-to-node.sock>\n";
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

    // Exchange thread maps with the server.
    auto constructReq = init.constructRequest();
    auto constructResp = constructReq.send().wait(io.waitScope);
    auto serverThreadMap = constructResp.getThreadMap();

    // Create a server-side thread we'll use for all calls.
    auto makeThreadReq = serverThreadMap.makeThreadRequest();
    makeThreadReq.setName("client");
    auto makeThreadResp = makeThreadReq.send().wait(io.waitScope);
    auto serverThread = makeThreadResp.getResult();

    // Get the Chain interface, passing the server thread as context.
    auto chainReq = init.makeChainRequest();
    chainReq.getContext().setThread(serverThread);
    auto chain = chainReq.send().wait(io.waitScope).getResult();

    // Query chain height.
    auto heightReq = chain.getHeightRequest();
    heightReq.getContext().setThread(serverThread);
    auto heightResp = heightReq.send().wait(io.waitScope);

    if (heightResp.getHasResult()) {
        std::cout << "Chain height: " << heightResp.getResult() << "\n";
    } else {
        std::cout << "No chain height available\n";
    }

    return 0;
}
