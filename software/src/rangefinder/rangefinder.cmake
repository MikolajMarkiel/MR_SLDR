
target_sources(${PROJECT_NAME}
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/rangefinder.c
  ${CMAKE_CURRENT_LIST_DIR}/rangefinder.h
)

# if(DEFINED VL53L0X)
#   message(STATUS "ADDING VL sensor")
# endif
