# PCB Vibration and Thermal Tester

## Overview
In industry, vibration/thermal testing involves simulating real-world conditions where a system is subjected to simultaneous temperature and vibration changes. This project involves developing a custom test setup where we can simulate these conditions in a controlled, reproducible environment. 

## Setup
 1. Go to the Github website view of this repository, and in the top right corner, create a
    clone of this template with the "Use This Template" button.
 2. Clone the newly created repository to your computer.
 3. Run the setup script (`./scripts/setup.sh`) with the new name of the project as its
    argument.
 4. Confirm that the project works by running `make`

## Flashing the board
### flash.sh will be different for mac/windows:
 For mac:

 ```
#!/usr/bin/env bash
 
# Load the project name from the Makefile
PROJECT_NAME=template
PROJECT_VERSION=f33
if [[ $(cat Makefile) =~ PROJECT_NAME[[:space:]]*:=[[:space:]]*([^[:space:]]*) ]]; then
    PROJECT_NAME=${BASH_REMATCH[1]}
fi
if [[ $(cat Makefile) =~ PROJECT_VERSION[[:space:]]*:=[[:space:]]*([^[:space:]]*) ]]; then
    PROJECT_VERSION=${BASH_REMATCH[1]}
fi
 
ELF=build/stm32/$PROJECT_NAME-$PROJECT_VERSION.elf

echo "Programming with $ELF"

openocd -f ./openocd.cfg -c "program ${ELF} verify reset" -c "exit"
 ```

## Requirements
### Windows-Specific:
- [Windows Subsystem for Linux](https://learn.microsoft.com/en-us/windows/wsl/install)
- [Docker Desktop](https://docs.docker.com/desktop/install/windows-install/)

Not strictly required, but advised, is Github Desktop.
