#include <cstring>
#include <future>
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

#include <mp/proxy-io.h>

#include <ipc/capnp/init.capnp.h>
#include <ipc/capnp/init.capnp.proxy.h>

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: echo <path-to-node.sock>\n";
        return 1;
    }

    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un addr{.sun_family = AF_UNIX};
    std::strncpy(addr.sun_path, argv[1], sizeof(addr.sun_path) - 1);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("connect");
        return 1;
    }

    std::promise<mp::EventLoop*> promise;
    std::thread loop_thread([&] {
        mp::EventLoop loop("echo", [](mp::LogMessage) {});
        promise.set_value(&loop);
        loop.loop();
    });
    auto* loop = promise.get_future().get();

    {
        auto init = mp::ConnectStream<ipc::capnp::messages::Init>(*loop, fd);
        auto echo = init->makeEcho();
        std::cout << echo->echo("hello from IPC!") << std::endl;
    }
    loop_thread.join();
    return 0;
}
