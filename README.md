# Smart CPR Training System – Firmware

This repository contains the embedded firmware for the Smart CPR Training System
developed as part of ENGR 498 (Senior Design) at the University of Arizona.

## Overview
The firmware runs on an Arduino Mega 2560 and computes CPR quality metrics
from sensor inputs (or scenario-based test data) and drives a pump-based
blood flow visualization system to provide real-time feedback to trainees.

## Hardware Platform
- Arduino Mega 2560
- Arduino Motor Shield Rev3
- External 12V DC pump (powered separately)
- Force sensing resistors (FSRs)
- Linear displacement sensor (potentiometer)

## Firmware Architecture
The firmware is organized into modular components:
- `main.cpp` – system initialization and control loop
- `dbp.*` – CPR quality / DBP computation
- `scenarios.*` – scenario-based testing framework
- `pump_control.*` – pump and motor shield control logic

## Build and Upload
The firmware is developed using PlatformIO and can be built and uploaded
using VS Code or the PlatformIO CLI targeting the Arduino Mega 2560.

## Project Status
This repository reflects the MVP1/MVP1.5 firmware state for the
Technical Data Package (TDP1). Live sensor integration and display
enhancements are planned future work.
