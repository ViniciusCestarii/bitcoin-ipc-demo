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

#include <iomanip>
#include <sstream>

#include <schemas/capnp/mp/proxy.capnp.h>
#include <schemas/capnp/init.capnp.h>
#include <schemas/capnp/chain.capnp.h>

class NotificationsImpl final : public ipc::capnp::messages::ChainNotifications::Server
{
public:
    kj::Promise<void> transactionAddedToMempool(TransactionAddedToMempoolContext context) override
    {
        auto tx = context.getParams().getTx();
        std::ostringstream hex;
        for (auto byte : tx) {
            hex << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        }
        std::cout << "transactionAddedToMempool: " << tx.size() << " bytes, raw=" << hex.str() << "\n";
        std::cout.flush();
        return kj::READY_NOW;
    }

    // Default no-op handlers for the other chain notifications. Overriding
    // them keeps the node from getting an "unimplemented method" error when it
    // fires a notification we don't care about yet.
    kj::Promise<void> transactionRemovedFromMempool(TransactionRemovedFromMempoolContext context) override { return kj::READY_NOW; }
    kj::Promise<void> blockConnected(BlockConnectedContext context) override { return kj::READY_NOW; }
    kj::Promise<void> blockDisconnected(BlockDisconnectedContext context) override { return kj::READY_NOW; }
    kj::Promise<void> updatedBlockTip(UpdatedBlockTipContext context) override { return kj::READY_NOW; }
    kj::Promise<void> chainStateFlushed(ChainStateFlushedContext context) override { return kj::READY_NOW; }
};

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

    auto makePoolReq = serverThreadMap.makePoolRequest();
    makePoolReq.setName("client");
    makePoolReq.setCount(2);
    makePoolReq.send().wait(io.waitScope);

    auto chainReq = init.makeChainRequest();
    auto chain = chainReq.send().wait(io.waitScope).getResult();

    auto handleNotificationReq = chain.handleNotificationsRequest();
    handleNotificationReq.setNotifications(kj::heap<NotificationsImpl>());
    // Keep the Handler alive for the lifetime of the program; dropping it
    // unregisters the notifications on the node side.
    auto handler = handleNotificationReq.send().wait(io.waitScope).getResult();

    std::cout << "Listening for transactionAddedToMempool notifications...\n";
    std::cout.flush();

    // Run the event loop forever so the node can call back into our capability.
    kj::NEVER_DONE.wait(io.waitScope);
}
