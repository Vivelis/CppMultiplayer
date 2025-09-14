# Network library
add_library(network STATIC
    Protocol.cpp
    NetworkManager.cpp
)

target_include_directories(network PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(network 
    Threads::Threads
)