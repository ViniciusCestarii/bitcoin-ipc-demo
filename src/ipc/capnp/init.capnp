# Copyright (c) 2021-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

@0xf2c5cfa319406aa6;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::capnp::messages");

using Proxy = import "./mp/proxy.capnp";
using Chain = import "chain.capnp";

interface Init $Proxy.wrap("interfaces::Init") {
    construct @0 (threadMap: Proxy.ThreadMap) -> (threadMap :Proxy.ThreadMap);
    # NOT USED IN THIS REPO
    makeEcho @1 (context :Proxy.Context) -> ();
    # NOT USED IN THIS REPO
    makeMining @3 (context :Proxy.Context) -> ();
    # NOT USED IN THIS REPO
    makeRpc @4 (context :Proxy.Context) -> ();

    makeChain @5 (context :Proxy.Context) -> (result :Chain.Chain);

    # DEPRECATED: no longer supported; server returns an error.
    makeMiningOld2 @2 () -> ();
}
