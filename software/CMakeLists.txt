# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

include(AppSettings.cmake)
include(dt/devicetree.cmake)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(blinky)

include(${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt)
# target_sources(app PRIVATE 
#   src/main.c
#   src/stepper.c
#   src/rangefinder.c
#   src/slider_bt.c
# )
