cmake_minimum_required(VERSION 3.0.0)
project(main VERSION 0.1.0)

include(CTest)
enable_testing()

link_libraries(swresample avformat avcodec avdevice avutil swscale pthread ssl crypto mpeg dl log4cpp)
link_directories(/home/boom/OKR2/include/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(tcp_server tcpServer.cpp Aes.hpp    sockets.hpp 
                          Rsa.hpp    Socket.hpp    log.hpp log.cpp
                          h264Handler.hpp mpeg2_ps.hpp 
                          aacHandler.hpp capRawData.hpp)

add_executable(tcp_client tcpClient.cpp Aes.hpp    sockets.hpp 
                            Rsa.hpp    Socket.hpp 
                            h264Handler.hpp mpeg2_ps.hpp 
                            aacHandler.hpp capRawData.hpp )

add_executable(TEST test.cpp aacHandler.hpp h264Handler.hpp)
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
