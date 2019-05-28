cmake_minimum_required(VERSION 3.8)
project(StackVM CXX ASM)

include_directories(include)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -Wall -Wextra -ggdb3 -Werror -Wpedantic -Wnull-dereference -Wuninitialized -Winit-self -Wmissing-include-dirs -Wunused -Wunknown-pragmas")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -Wextra -Wpedantic -Wnull-dereference -Wuninitialized -Winit-self -Wmissing-include-dirs -Wunused -Wunknown-pragmas")

#find_package(Boost 1.65 COMPONENTS program_options filesystem REQUIRED)
#include_directories(${Boost_INCLUDE_DIR})

add_executable(asm src/assembler_main.cpp src/assembler.cpp src/object.cpp)
add_executable(ld src/linker_main.cpp src/linker.cpp src/object.cpp)
add_executable(objdump src/objdump_main.cpp src/object.cpp src/objdump.cpp)
add_executable(vm src/virtual_machine_main.cpp src/ram.cpp src/virtual_machine.cpp src/processor.cpp src/object.cpp)
add_executable(validator src/instruction_set_validator.cpp)
add_executable(jit src/jit_main.cpp src/jit_compiler.cpp src/context_switch.s src/object.cpp src/func_call.s)

#target_link_libraries(asm ${Boost_LIBRARIES})
