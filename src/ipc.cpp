#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>

#include <capnp/dynamic.h>
#include <capnp/pretty-print.h>
#include <capnp/rpc-twoparty.h>
#include <kj/async-io.h>

#include <schemas/capnp/mp/proxy.capnp.h>
#include <schemas/capnp/init.capnp.h>
#include <schemas/capnp/noderpc.capnp.h>

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "Usage: bitcoin-ipc <path-to-node.sock> <method> [args...]\n";
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

    auto nodeRpcReq = init.makeNodeRpcRequest();
    nodeRpcReq.getContext().setThread(serverThread);
    auto nodeRpc = nodeRpcReq.send().wait(io.waitScope).getResult();

    // Pretty-print any response's `result` struct generically
    auto prettyResult = [&](auto&& resp) {
        return capnp::prettyPrint(capnp::toDynamic(resp.getResult())).flatten();
    };

    using Handler = std::function<kj::String()>;
    std::map<std::string, Handler> handlers;

    handlers["getNetworkInfo"] = [&] {
        auto req = nodeRpc.getNetworkInfoRequest();
        req.getContext().setThread(serverThread);
        return prettyResult(req.send().wait(io.waitScope));
    };

    handlers["getDeploymentInfo"] = [&] {
        auto req = nodeRpc.getDeploymentInfoRequest();
        req.getContext().setThread(serverThread);
        return prettyResult(req.send().wait(io.waitScope));
    };

    handlers["getBlockchainInfo"] = [&] {
        auto req = nodeRpc.getBlockchainInfoRequest();
        req.getContext().setThread(serverThread);
        return prettyResult(req.send().wait(io.waitScope));
    };

    handlers["estimateSmartFee"] = [&] {
        // Optional args: <confTarget> <conservative>, defaulting to 6 blocks and true.
        int confTarget = argc > 3 ? std::stoi(argv[3]) : 6;
        bool conservative = argc > 4 ? (std::string(argv[4]) == "true" || std::string(argv[4]) == "1") : true;

        auto req = nodeRpc.estimateSmartFeeRequest();
        req.getContext().setThread(serverThread);
        req.setConfTarget(confTarget);
        req.setConservative(conservative);
        return prettyResult(req.send().wait(io.waitScope));
    };

    std::string method = argv[2];
    auto it = handlers.find(method);
    if (it == handlers.end()) {
        std::cerr << "Unknown method: " << method << "\n";
        std::cerr << "Available methods:\n";
        for (const auto& [name, _] : handlers) {
            std::cerr << "  - " << name << "\n";
        }
        return 1;
    }

    std::cout << it->second().cStr() << "\n";
    return 0;
}
