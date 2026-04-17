cmake_minimum_required(VERSION 4.0)
project(bitcoin_ipc_demo CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

include(FetchContent)
FetchContent_Declare(Libmultiprocess
  GIT_REPOSITORY https://github.com/bitcoin-core/libmultiprocess.git
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(Libmultiprocess)

include(${libmultiprocess_SOURCE_DIR}/cmake/TargetCapnpSources.cmake)

find_package(Threads REQUIRED)

add_executable(echo src/echo.cpp)
target_capnp_sources(echo ${CMAKE_CURRENT_SOURCE_DIR}/src
  src/ipc/capnp/echo.capnp
  src/ipc/capnp/init.capnp
)
target_include_directories(echo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(echo PRIVATE Threads::Threads)
