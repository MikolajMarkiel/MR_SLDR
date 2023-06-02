# WARNING!

The project is in build stage. Missing documentation and functionalities will be added in time.

# Overview

The MR_SLDR is a project of remote controlled slider for camera. It will be controlled by Bluetooth Low Energy protocol. The main library source for this project is Zephyr OS.

# How to run

# Software description

## Web_bluetooth

## timer

Due to problems with flexibility occurence of threads in zephyr (minimal period step is 100us) I've decided to leave control of stepper motor up to hardware timer peripheral. Unfortunately right now esp32 timer drivers are dedicated only for counters that lose synchronization after overflow of counted ticks. Because of that I had to add extra modification to counter_esp32_tmr.c file (this file is a driver to handle every action of timer after setting devicetree &timerX status = "okay"). The modification allow us to decide that counter will autoreset after getting alarm or won't. It doesn't harm any project that use old driver, because for this to work user has to add extra `COUNTER_AUTORESET` config to Kconfig file. I've included new counter_esp32_tmr.c file in bin/ directory. The path for replacement is `../zephyrproject/zephyr/drivers/counter/counter_esp32_tmr.c`

# Hardware description

## Power section

The main supply for the board is +5V microUSB port. It supplies two DC-DC converters: step-up to +12V and step-down to +3,3V. The +12 DC voltage is needed for charging alternative power supply based on 3 li-ion cells driven by BMS3451 BMS. If there is no USB cable connected, then step-down converter is supplied by +12V instead. There is an external switch that enables step-down converter. If it is disabled, then every peripheral would be powered down except stepper motor driver which would go into sleep mode instead. Connecting USB cable with switch turned off allow charging batteries without turning on the device.
Voltage level of cell block is continually checked by ADC, and the feedback information is given to user via BLE.

## CPU & programming

An ESP32 module has been chosen as a CPU for the device. It has been because of his low cost and support for bluetooth communication and zephyr OS. There are two ways to communicate PC with CPU: via CP2102 USB to UART bridge module connected to micro USB and JTAG connector. Both allow to program flash memory. Additionally the bridge provides serial communication with PC and JTAG give possibility to debug software.

## Stepper motor driver

DRV8825 module has been used as an output driver for stepper motor. It provides easy interface because of it's STEP/DIR functionality. For more information please refer to datasheet of module.

## Distance meter

For checking actual position of camera even after power off the laser distance meter VL53LXX module is connected to CPU by I2C interface. Because it is needed to connect module directly at path of moving camera, then the module has been removed from the main hardware PCB and case. For more information please refer to datasheet of module.

## Camera control blocks

There are also two isolated switches for example for activating remote control of camera or other these types activities. They are designed with use of single channel transoptors PC817.

## EEPROM

The board provides also external EEPROM (M93C76C module) for saving user data of slider settings. It communicates with CPU via SPI interface.

## Mechanicals

The board has been designed for Z57J universal case (https://www.kradex.com.pl/product/enclosures_hermetically_sealed/z57j_ps?lang=en) but it is also possible to print something similar. By now there is no need to provide additional 3D models of case.
