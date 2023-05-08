# WARNING!

The project is in build stage. Missing documentation and functionalities will be added in time.

# Overview

The MR_SLDR is a project of remote controlled slider for camera. It will be controlled by Bluetooth Low Energy protocol. The main library source for this project is Zephyr OS.

# How to run

# Software description

## Web_bluetooth

## timer

Due to problems with flexibility occurence of threads in zephyr (minimal period step is 100us) I've decided to leave control of stepper motor up to hardware timer peripheral. Unfortunately right now esp32 timer drivers are dedicated only for counters that lose synchronization after overflow of counted ticks. Because of that I had to add extra modification to counter_esp32_tmr.c file (this file is a driver to handle every action of timer after setting devicetree &timer<x> status = "okay"). The modification allow us to decide that counter will autoreset after getting alarm or won't. It doesn't harm any project that use old driver, because for this to work user has to add extra `COUNTER_AUTORESET` config to Kconfig file. I've included new counter_esp32_tmr.c file in bin/ directory. The path for replacement is `../zephyrproject/zephyr/drivers/counter/counter_esp32_tmr.c`

# Hardware description
