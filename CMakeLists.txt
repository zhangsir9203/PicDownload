cmake_minimum_required(VERSION 3.5)

project(PicDowndowner)

set(CMAKE_CXX_STANDARD 11)


set(INC_DIR
    ${PROJECT_SOURCE_DIR}/.
    )

include_directories(${INC_DIR})

add_definitions(-DOS_EMBEDDED_LINUX)

set(ALL_SRCS
    ${PROJECT_SOURCE_DIR}/main.cc
    ${PROJECT_SOURCE_DIR}/picdownloader.cc
    )

add_executable(${PROJECT_NAME} ${ALL_SRCS})
target_link_libraries(${PROJECT_NAME} pthread boost_system boost_thread boost_filesystem boost_regex curl)

