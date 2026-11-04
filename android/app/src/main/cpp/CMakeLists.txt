cmake_minimum_required(VERSION 3.22)
project(gdrpc CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(discord_partner_sdk REQUIRED CONFIG)

add_library(gdrpc SHARED jni.cpp presence.cpp)
target_link_libraries(gdrpc discord_partner_sdk::discord_partner_sdk log)
