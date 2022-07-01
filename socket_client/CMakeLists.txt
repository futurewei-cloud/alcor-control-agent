set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../build/bin)

find_library(
        LIBBPF_LIBRARIES
        NAMES
            bpf
        PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/../include NO_DEFAULT_PATH
)

link_libraries(/usr/local/lib/libbpf.a)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include/libbpf/usr/include/bpf/)
#include_directories(/home/ubuntu/downloads/libbpf/src)

get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
foreach(dir ${dirs})
    message(STATUS "dir='${dir}'")
endforeach()


project(socket_client C)

add_executable(socket_client socket_client.c)

#add_dependencies(socket_client libbpf)
#
#target_link_libraries(socket_client -l:libbpf.a)