cmake_minimum_required(VERSION 3.23)
project(Cache)

set(CMAKE_CXX_STANDARD 17)

add_executable(Cache main.cpp threadPool.h scheduler.h fileManager.h Log.h test.h buffer.h)
