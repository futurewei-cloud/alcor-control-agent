cmake_minimum_required(VERSION 3.10)
project(networkControlAgent)

# Set the version number.
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 14)
set(CPPKAFKA_VERSION_MAJOR 0)
set(CPPKAFKA_VERSION_MINOR 3)
set(CPPKAFKA_VERSION_REVISION 1)
set(CPPKAFKA_VERSION "${CPPKAFKA_VERSION_MAJOR}.${CPPKAFKA_VERSION_MINOR}.${CPPKAFKA_VERSION_REVISION}")
set(RDKAFKA_MIN_VERSION 0x00090400)

add_custom_target(
    Transit ALL
    COMMAND /usr/bin/make
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Transit/
)

add_subdirectory(src)
add_subdirectory(test)
