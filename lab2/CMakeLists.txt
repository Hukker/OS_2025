cmake_minimum_required(VERSION 3.10)
project(LabChat VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -pthread")

file(GLOB CONN_FILES "conn_*.cpp")

if(NOT CONN_FILES)
    message(FATAL_ERROR "Не найдены файлы соединений с префиксом 'conn_'")
endif()

foreach(conn_file ${CONN_FILES})
    get_filename_component(TYPE_CODE ${conn_file} NAME_WE)
    string(REPLACE "conn_" "" TYPE_CODE ${TYPE_CODE})
    add_executable(host_${TYPE_CODE} host.cpp ${conn_file})
    target_link_libraries(host_${TYPE_CODE} pthread rt)
    add_executable(client_${TYPE_CODE} client.cpp ${conn_file})
    target_link_libraries(client_${TYPE_CODE} pthread rt)
    list(APPEND ALL_HOSTS host_${TYPE_CODE})
    list(APPEND ALL_CLIENTS client_${TYPE_CODE})
endforeach()

add_custom_target(build_all_hosts DEPENDS ${ALL_HOSTS})
add_custom_target(build_all_clients DEPENDS ${ALL_CLIENTS})
add_custom_target(build_all DEPENDS build_all_hosts build_all_clients)
add_custom_target(all_build DEPENDS build_all)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY DEFAULT_TARGET all_build)