cmake_minimum_required(VERSION 4.0)
project(bitcoin-chain-ipc CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

find_package(Threads REQUIRED)
find_package(CapnProto REQUIRED)

set(CAPNP_SCHEMAS
  src/ipc/capnp/common.capnp
  src/ipc/capnp/handler.capnp
  src/ipc/capnp/init.capnp
  src/ipc/capnp/chain.capnp
  src/ipc/capnp/mp/proxy.capnp
)

capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS ${CAPNP_SCHEMAS})

add_executable(bitcoin-chain-ipc src/chain-ipc.cpp ${CAPNP_SRCS})

target_link_libraries(bitcoin-chain-ipc PRIVATE Threads::Threads CapnProto::capnp-rpc)
target_include_directories(bitcoin-chain-ipc PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/src)
