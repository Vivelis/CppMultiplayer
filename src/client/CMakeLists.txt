# Client executable
add_executable(client
    GameClient.cpp
    main.cpp
)

target_include_directories(client PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(client
    network
    Threads::Threads
)