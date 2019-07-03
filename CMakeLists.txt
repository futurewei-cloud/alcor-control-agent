cmake_minimum_required(VERSION 3.10)
project(networkControlAgent)

# Set the version number.
set(CPPKAFKA_VERSION_MAJOR 0)
set(CPPKAFKA_VERSION_MINOR 3)
set(CPPKAFKA_VERSION_REVISION 1)
set(CPPKAFKA_VERSION "${CPPKAFKA_VERSION_MAJOR}.${CPPKAFKA_VERSION_MINOR}.${CPPKAFKA_VERSION_REVISION}")
set(RDKAFKA_MIN_VERSION 0x00090400)
set(CMAKE_CXX_STANDARD 11)


FIND_LIBRARY(RDKAFKA rdkafka /usr/lib/x86_64-linux-gnu NO_DEFAULT_PATH)
FIND_LIBRARY(CPPKAFKA cppkafka /usr/local/lib64 NO_DEFAULT_PATH)
link_libraries(${RDKAFKA} ${CPPKAFKA})
include_directories("/usr/include/librdkafka")
include_directories("/usr/local/include/cppkafka")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(networkControlAgent src/comm/messageproducer.cpp)
target_link_libraries(networkControlAgent cppkafka)
target_link_libraries(networkControlAgent rdkafka)
