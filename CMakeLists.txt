cmake_minimum_required(VERSION 3.30)
project(bitcoin_ipc_demo CXX)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

# Disable everything in bitcoin core we don't need for an IPC client.
# These must be set before FetchContent_MakeAvailable(Bitcoin).
set(BUILD_BITCOIN_BIN OFF CACHE BOOL "" FORCE)
set(BUILD_DAEMON      OFF CACHE BOOL "" FORCE)
set(BUILD_CLI         OFF CACHE BOOL "" FORCE)
set(BUILD_TX          OFF CACHE BOOL "" FORCE)
set(BUILD_UTIL        OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS       OFF CACHE BOOL "" FORCE)
set(BUILD_BENCH       OFF CACHE BOOL "" FORCE)
set(BUILD_GUI         OFF CACHE BOOL "" FORCE)
set(ENABLE_WALLET     OFF CACHE BOOL "" FORCE)
set(INSTALL_MAN       OFF CACHE BOOL "" FORCE)
set(ENABLE_EXTERNAL_SIGNER OFF CACHE BOOL "" FORCE)
set(REDUCE_EXPORTS    ON  CACHE BOOL "" FORCE)
# ENABLE_IPC defaults ON on non-Windows; leave it alone.

include(FetchContent)
FetchContent_Declare(Bitcoin
    GIT_REPOSITORY https://github.com/bitcoin/bitcoin.git
    GIT_TAG        313aaa0eae4446533b3668360ae0a353cde63312 # refs/pull/34020/head
    GIT_SHALLOW    TRUE
    GIT_CONFIG     "remote.origin.fetch=+refs/pull/34020/head:refs/remotes/origin/pr/34020"
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(Bitcoin)

# Work around libmultiprocess's target_capnp_sources computing its generated-
# header include dir relative to the top-level project, which misses when
# bitcoin is pulled in via FetchContent. Point bitcoin_ipc at the real
# location of its generated headers.
target_include_directories(bitcoin_ipc PUBLIC
    ${bitcoin_BINARY_DIR}/src/ipc
)

find_package(Threads REQUIRED)

add_executable(mining_demo src/mining_demo.cpp)

# Bitcoin's capnp-generated headers use includes like <capnp/init.capnp.h>,
# rooted at src/ipc.
target_include_directories(mining_demo PRIVATE
    ${bitcoin_SOURCE_DIR}/src
    ${bitcoin_SOURCE_DIR}/src/ipc
    ${bitcoin_BINARY_DIR}/src
    ${bitcoin_BINARY_DIR}/src/ipc
)

target_link_libraries(mining_demo PRIVATE
    bitcoin_ipc
    bitcoin_common
    multiprocess
    Threads::Threads
)
