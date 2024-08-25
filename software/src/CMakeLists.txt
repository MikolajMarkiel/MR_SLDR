
target_sources(${PROJECT_NAME}
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/main.c
)

message(STATUS "PROJECT NAME: ${PROJECT_NAME}")

include(${CMAKE_CURRENT_LIST_DIR}/rangefinder/rangefinder.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/slider_bt/slider_bt.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/stepper/stepper.cmake)
