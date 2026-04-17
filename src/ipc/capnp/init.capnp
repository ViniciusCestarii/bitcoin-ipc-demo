@0xf2c5cfa319406aa6;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::capnp::messages");

using Proxy = import "/mp/proxy.capnp";
$Proxy.include("interfaces/echo.h");
$Proxy.include("interfaces/init.h");
$Proxy.includeTypes("ipc/capnp/init-types.h");

using Echo = import "echo.capnp";

interface Init $Proxy.wrap("interfaces::Init") {
    construct @0 (threadMap: Proxy.ThreadMap) -> (threadMap :Proxy.ThreadMap);
    makeEcho @1 (context :Proxy.Context) -> (result :Echo.Echo);
}
