#include <cstring>
#include <future>
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <mp/proxy-io.h>

#include <capnp/init.capnp.h>
#include <capnp/init.capnp.proxy.h>
#include <capnp/mining.capnp.proxy.h>

#include <interfaces/init.h>
#include <interfaces/mining.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <primitives/transaction_identifier.h>

constexpr auto ZERO_HEX = "0000000000000000000000000000000000000000000000000000000000000000";

template <typename Results>
void print_results(const std::string& label, const Results& txs)
{
    for (size_t i = 0; i < txs.size(); ++i) {
        const auto& tx = txs[i];
        std::cout << label << "[" << i << "] ";
        if (tx) {
            std::cout << "hash=" << tx->GetHash().ToString()
                      << " vin=" << tx->vin.size()
                      << " vout=" << tx->vout.size() << "\n";
        } else {
            std::cout << "<not found>\n";
        }
    }
}

void demo_get_by_txid(interfaces::Mining& mining)
{
    auto present = Txid::FromHex("771dbe406037ce5ce81dcd6e3ebc4455250d773acd11aec50bdfd767b6581428");
    auto missing = Txid::FromHex(ZERO_HEX);
    if (!present || !missing) {
        std::cerr << "invalid txid hex\n";
        return;
    }
    std::vector<Txid> txids{*present, *missing};
    print_results("txid", mining.getTransactionsByTxID(txids));
}

void demo_get_by_wtxid(interfaces::Mining& mining)
{
    auto present = Wtxid::FromHex("31cb811864014697a782cc0f5b400e34b61448ab99be6b957a0288f09ad30530");
    auto missing = Wtxid::FromHex(ZERO_HEX);
    if (!present || !missing) {
        std::cerr << "invalid wtxid hex\n";
        return;
    }
    std::vector<Wtxid> wtxids{*present, *missing};
    print_results("wtxid", mining.getTransactionsByWitnessID(wtxids));
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: mining_demo <path-to-node.sock>\n";
        return 1;
    }

    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, argv[1], sizeof(addr.sun_path) - 1);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("connect");
        return 1;
    }

    std::promise<mp::EventLoop*> promise;
    std::thread loop_thread([&] {
        mp::EventLoop loop("mining-demo", [](mp::LogMessage) {});
        promise.set_value(&loop);
        loop.loop();
    });
    auto* loop = promise.get_future().get();

    {
        auto init = mp::ConnectStream<ipc::capnp::messages::Init>(*loop, fd);

        auto mining = init->makeMining();
        if (!mining) {
            std::cerr << "makeMining() returned null — server does not expose Mining interface\n";
            return 1;
        }

        demo_get_by_txid(*mining);
        demo_get_by_wtxid(*mining);
    }

    loop_thread.join();
    return 0;
}
