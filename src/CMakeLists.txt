

set(GENCP_SRCS 
    gencp.cpp
    file_access.cpp)

add_library(alvium_file_access STATIC ${GENCP_SRCS})
target_include_directories(alvium_file_access PUBLIC ${CMAKE_SOURCE_DIR}/include)