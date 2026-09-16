# SimpleMatchmaker server, as vendored into OpenSWOS.
#
# Upstream built its own bundled ENET copy. We build against the ONE vendored
# ENET in third_party/enet instead (see OPENSWOS.md): the game's matchmaking
# client links that same copy, and two ENETs in one tree is how you get a
# wire-compatible-but-subtly-different pair.
#
# Only the SERVER is built here. The client half (ServerConnection/,
# P2PConnection/) is vendored as READABLE REFERENCE for src/mm_client.c and is
# deliberately not compiled: it is C++ and Windows-leaning (Sleep(), conio.h),
# while our client is C and has to build for every SDL target.
#
#   cmake -S third_party/matchmaker -B build/matchmaker -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/matchmaker -j
#
# scripts/run_matchmaker.sh does both and then runs it.
cmake_minimum_required(VERSION 3.10)
project(SwosMatchmaker CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ENET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../enet")

# ENET's build knobs are normally discovered by autotools/CMake feature checks.
# The set below is what its own CMakeLists probes for on a modern Linux/macOS;
# getting one wrong silently changes socket behaviour rather than failing to
# build, so they are listed explicitly instead of inherited.
set(ENET_SOURCES
    ${ENET_DIR}/callbacks.c
    ${ENET_DIR}/compress.c
    ${ENET_DIR}/host.c
    ${ENET_DIR}/list.c
    ${ENET_DIR}/packet.c
    ${ENET_DIR}/peer.c
    ${ENET_DIR}/protocol.c
)
if(WIN32)
    list(APPEND ENET_SOURCES ${ENET_DIR}/win32.c)
else()
    list(APPEND ENET_SOURCES ${ENET_DIR}/unix.c)
endif()

add_library(swos_enet STATIC ${ENET_SOURCES})
target_include_directories(swos_enet PUBLIC ${ENET_DIR}/include)
if(NOT WIN32)
    target_compile_definitions(swos_enet PUBLIC
        HAS_SOCKLEN_T=1 HAS_POLL=1 HAS_FCNTL=1
        HAS_INET_PTON=1 HAS_INET_NTOP=1 HAS_MSGHDR_FLAGS=1)
endif()

file(GLOB server_sources "${CMAKE_CURRENT_SOURCE_DIR}/Server/*.cpp")
file(GLOB common_sources "${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp")

add_executable(swos-matchmaker ${server_sources} ${common_sources})
target_include_directories(swos-matchmaker PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Server/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/common/include")
target_link_libraries(swos-matchmaker swos_enet)

if(WIN32)
    target_link_libraries(swos-matchmaker winmm ws2_32)
endif()

# The reference CLIENT, built only as a test peer (tools/mm_refpeer). Our client
# is src/mm_client.c and stays the only client the game ships; this exists so the
# two implementations can be pointed at each other -- see OPENSWOS.md, "The
# reference client as a test peer". It is not part of any game build.
add_executable(mm-refpeer
    "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/mm_refpeer/mm_refpeer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ServerConnection/ServerConnection.cpp"
    ${common_sources})
target_include_directories(mm-refpeer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/ServerConnection"
    "${CMAKE_CURRENT_SOURCE_DIR}/common/include")
target_link_libraries(mm-refpeer swos_enet)
if(WIN32)
    target_link_libraries(mm-refpeer winmm ws2_32)
endif()
