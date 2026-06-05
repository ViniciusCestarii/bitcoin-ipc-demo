cmake_minimum_required(VERSION 4.0)
project(bitcoin-ipc CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

find_package(Threads REQUIRED)
find_package(CapnProto REQUIRED)

set(CAPNP_SCHEMAS
  src/ipc/capnp/common.capnp
  src/ipc/capnp/echo.capnp
  src/ipc/capnp/init.capnp
  src/ipc/capnp/nodeinfo.capnp
  src/ipc/capnp/mining.capnp
  src/ipc/capnp/mp/proxy.capnp
)

capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS ${CAPNP_SCHEMAS})

add_executable(bitcoin-ipc src/ipc.cpp ${CAPNP_SRCS})

target_link_libraries(bitcoin-ipc PRIVATE Threads::Threads CapnProto::capnp-rpc)
target_include_directories(bitcoin-ipc PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/src)
