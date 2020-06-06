cmake_minimum_required(VERSION 3.10)

project(Sphere)

set (CMAKE_CXX_STANDARD 17)
add_executable(sphere sphere.cxx)
install(TARGETS sphere  DESTINATION bin)

