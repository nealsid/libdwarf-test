cmake_minimum_required(VERSION 3.24)

project(libdwarf-test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_DEPLOYMENT_TARGET 13)

link_directories("/Users/nealsid/src/github/libdwarf-code/build/src/lib/libdwarf")
add_executable(libdwarf-test libdwarf-test.cc)

target_include_directories(libdwarf-test PRIVATE "/Users/nealsid/src/github/libdwarf-code/src/lib/libdwarf")
target_link_libraries(libdwarf-test dwarf z)
