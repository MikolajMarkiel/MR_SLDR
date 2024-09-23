# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)


find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# set(PROJECT_NAME MR_SLDR)

project(MR_SLDR)

target_sources(app PRIVATE 
  src/main.c
  src/slider.c
  src/slider.h
  src/stepper_motor.c
  src/stepper_motor.h
  src/led_manager.c
  src/led_manager.h
  # src/rangefinder.c
  # src/rangefinder.h
  src/slider_bt.c
  src/slider_bt.h
)

# Optionally, you can create a custom target just for creating the symlink
add_custom_target(CreateCompileCommandsSymlink ALL
    DEPENDS ${CMAKE_BINARY_DIR}/compile_commands.json
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "Creating symbolic link for compile_commands.json in the project root"
)

# Link for purpose of `west espressif monitor` command
add_custom_target(CreateBuildSymlink ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/build
    COMMENT "Creating symbolic link 'build' pointing to the current build directory"
)
