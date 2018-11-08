cmake_minimum_required(VERSION 3.8)
project(StackVM)

include_directories(include)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -O0 -Wall -Wextra -g -Werror -Wpedantic -Wnull-dereference -Wuninitialized -Winit-self -Wmissing-include-dirs -Wunused -Wunknown-pragmas")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -Wextra -Wpedantic -Wnull-dereference -Wuninitialized -Winit-self -Wmissing-include-dirs -Wunused -Wunknown-pragmas")

#find_package(Boost 1.65 COMPONENTS program_options filesystem REQUIRED)
#include_directories(${Boost_INCLUDE_DIR})

add_executable(asm src/assembler_main.cpp src/assembler.cpp src/object.cpp)
add_executable(objdump src/objdump_main.cpp src/object.cpp)
add_executable(vm src/virtual_machine_main.cpp)
add_executable(validator src/instruction_set_validator.cpp)

#target_link_libraries(asm ${Boost_LIBRARIES})
