cmake_minimum_required(VERSION 3.10)
project(alvium_user_data_access C CXX)

add_compile_options($<$<OR:$<CONFIG:RelWithDebInfo>,$<CONFIG:Release>>:-O3>)

include_directories(third_party/cppcrc)

add_subdirectory(src)
add_subdirectory(tools)