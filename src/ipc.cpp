#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>

#include <capnp/rpc-twoparty.h>
#include <kj/async-io.h>

#include <ipc/capnp/mp/proxy.capnp.h>
#include <ipc/capnp/init.capnp.h>
#include <ipc/capnp/nodeinfo.capnp.h>

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

    auto constructReq = init.constructRequest();
    auto constructResp = constructReq.send().wait(io.waitScope);
    auto serverThreadMap = constructResp.getThreadMap();

    auto makeThreadReq = serverThreadMap.makeThreadRequest();
    makeThreadReq.setName("client");
    auto makeThreadResp = makeThreadReq.send().wait(io.waitScope);
    auto serverThread = makeThreadResp.getResult();

    auto nodeInfoReq = init.makeNodeInfoRequest();
    nodeInfoReq.getContext().setThread(serverThread);
    auto nodeInfo = nodeInfoReq.send().wait(io.waitScope).getResult();

    auto deploymentInfoReq = nodeInfo.getDeploymentInfoRequest();
    deploymentInfoReq.getContext().setThread(serverThread);
    auto deploymentInfoResp = deploymentInfoReq.send().wait(io.waitScope);

    std::cout << "Chain height: " << deploymentInfoResp.getResult().getHeight() << "\n";

    return 0;
}
