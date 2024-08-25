message(STATUS "Include devicetree")

macro(AddIf cond name val)
  if(DEFINED ${cond})
    list(APPEND ${name} ${val})
  endif()
endmacro()

AddIf(ESP32         dt ${CMAKE_CURRENT_LIST_DIR}/esp32.overlay)
AddIf(TIMER0        dt ${CMAKE_CURRENT_LIST_DIR}/timer0.overlay)
AddIf(VL53L0X       dt ${CMAKE_CURRENT_LIST_DIR}/vl53l0x.overlay)

set(DTC_OVERLAY_FILE ${dt})

