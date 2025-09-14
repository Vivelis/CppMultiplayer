# Server executable
add_executable(server
    GameServer.cpp
    main.cpp
)

target_include_directories(server PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(server
    network
    Threads::Threads
)