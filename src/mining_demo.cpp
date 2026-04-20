#include <cstring>
#include <future>
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

#include <mp/proxy-io.h>

#include <capnp/init.capnp.h>
#include <capnp/init.capnp.proxy.h>
#include <capnp/mining.capnp.proxy.h>

#include <interfaces/init.h>
#include <interfaces/mining.h>
#include <primitives/block.h>

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

        if (auto tip = mining->getTip()) {
            std::cout << "tip height=" << tip->height
                      << " hash=" << tip->hash.ToString() << "\n";
        } else {
            std::cout << "tip: <none>\n";
        }

        auto tmpl = mining->createNewBlock({}, /*cooldown=*/false);
        if (!tmpl) {
            std::cerr << "createNewBlock() returned null\n";
            return 1;
        }

        CBlock block = tmpl->getBlock();
        std::cout << "block version=" << block.nVersion
                  << " prev=" << block.hashPrevBlock.ToString()
                  << " tx_count=" << block.vtx.size() << "\n";
    }

    loop_thread.join();
    return 0;
}
