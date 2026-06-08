#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>
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

    // Decode a display block hash (big-endian hex) into the raw internal byte
    // order expected by the IPC `Data` field (reversed).
    auto hashHexToBytes = [](const std::string& hex) {
        std::vector<kj::byte> bytes;
        bytes.reserve(hex.size() / 2);
        for (size_t i = 0; i + 1 < hex.size(); i += 2) {
            bytes.push_back(static_cast<kj::byte>(std::stoi(hex.substr(i, 2), nullptr, 16)));
        }
        std::reverse(bytes.begin(), bytes.end());
        return bytes;
    };

    // Encode raw bytes (e.g. a serialized block) as hex.
    auto bytesToHex = [](capnp::Data::Reader data) {
        static const char* digits = "0123456789abcdef";
        kj::Vector<char> hex(data.size() * 2);
        for (auto b : data) {
            hex.add(digits[b >> 4]);
            hex.add(digits[b & 0x0f]);
        }
        return kj::str(hex.asPtr());
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

    handlers["getBestBlockHash"] = [&] {
        auto req = nodeRpc.getBestBlockHashRequest();
        req.getContext().setThread(serverThread);
        return kj::str(req.send().wait(io.waitScope).getResult());
    };

    handlers["getBlockHash"] = [&] {
        // Required arg: <height>
        if (argc < 4) {
            std::cerr << "getBlockHash requires <height>\n";
            return kj::str();
        }
        int height = std::stoi(argv[3]);

        auto req = nodeRpc.getBlockHashRequest();
        req.getContext().setThread(serverThread);
        req.setHeight(height);
        return kj::str(req.send().wait(io.waitScope).getResult());
    };

    handlers["getBlockHeader"] = [&] {
        // Required arg: <blockHash> (display hex)
        if (argc < 4) {
            std::cerr << "getBlockHeader requires <blockHash>\n";
            return kj::str();
        }
        auto hash = hashHexToBytes(argv[3]);

        auto req = nodeRpc.getBlockHeaderRequest();
        req.getContext().setThread(serverThread);
        req.setBlockHash(kj::arrayPtr(hash.data(), hash.size()));
        return prettyResult(req.send().wait(io.waitScope));
    };

    handlers["getBlock"] = [&] {
        // Required arg: <blockHash> (display hex); result is the raw block as hex.
        if (argc < 4) {
            std::cerr << "getBlock requires <blockHash>\n";
            return kj::str();
        }
        auto hash = hashHexToBytes(argv[3]);

        auto req = nodeRpc.getBlockRequest();
        req.getContext().setThread(serverThread);
        req.setBlockHash(kj::arrayPtr(hash.data(), hash.size()));
        return bytesToHex(req.send().wait(io.waitScope).getResult());
    };

    handlers["getTxOut"] = [&] {
        // Required args: <txid> (display hex) <n>; optional: <includeMempool> (default true).
        if (argc < 5) {
            std::cerr << "getTxOut requires <txid> <n> [includeMempool]\n";
            return kj::str();
        }
        auto txid = hashHexToBytes(argv[3]);
        uint32_t n = static_cast<uint32_t>(std::stoul(argv[4]));
        bool includeMempool = argc > 5 ? (std::string(argv[5]) == "true" || std::string(argv[5]) == "1") : true;

        auto req = nodeRpc.getTxOutRequest();
        req.getContext().setThread(serverThread);
        req.setTxid(kj::arrayPtr(txid.data(), txid.size()));
        req.setN(n);
        req.setIncludeMempool(includeMempool);
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
