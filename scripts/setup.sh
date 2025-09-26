#!/usr/bin/env bash

# If the name of the project is provided as the first argument, update the
# name in the Makefile. From there, other scripts can load the name
if [ -n $1 ]; then
    sed -i "s/PROJECT_NAME\s*:=\s*.*/PROJECT_NAME := $1/" Makefile
fi

# No need to initialize the FreeRTOS/HAL modules in the core
git submodule update --init lib/Core
git submodule update --init --recursive lib/FreeRTOS-Kernel lib/STM32CubeG4
docker build --build-arg=UID=$(id -u) --build-arg=UNAME=$(id -nu) ./docker/ -t temp
