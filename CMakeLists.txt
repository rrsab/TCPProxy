cmake_minimum_required(VERSION 3.22)
project(TcpProxy)

set(CMAKE_CXX_STANDARD 98)

add_executable(TcpProxy tcpproxy_server.cpp.cpp)
