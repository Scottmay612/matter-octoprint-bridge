# Matter-to-OctoPrint Bridge

This repository contains the custom source code for my Matter-to-OctoPrint smart home integration. It allows an Ender 3 Pro running OctoPrint to connect natively to Google Home via the Matter protocol, entirely bypassing the need for third-party cloud servers or heavy Homebridge plugins.

**Note for grading/review:** This repo only contains the specific custom files I wrote and modified. It does not include the full 10GB Matter SDK required to actually compile the C++ application. 

## Architecture

The bridge runs locally on a Raspberry Pi using a hybrid architecture:

1. **C++ Matter Controller (`main.cpp`)**: A heavily modified version of the Matter SDK's `chip-lighting-app`. It acts as the Matter endpoint (spoofing a dimmable lightbulb for print progress and thermostats for the hotend/bed). It communicates directly with the Google Nest Hub over local Wi-Fi.
2. **Python API Scripts**: The C++ app constantly polls a suite of lightweight Python scripts. These scripts handle the actual HTTP REST API calls to the local OctoPrint server.

## File Overview

* `main.cpp`: The core C++ application loop. In a full build environment, this replaces the default `main.cpp` in the Matter SDK's `examples/lighting-app/linux` directory. It contains the 5-second API polling loop and the Matter attribute change callbacks.
* `lighting-app.zap`: The ZCL configuration file that defines the custom Matter endpoints and clusters. This replaces the `connectedhomeip/examples/lighting-app/lighting-common/lighting-app.zap` file.
* `*.py`: The Python scripts called by the C++ backend. These translate the Matter commands into OctoPrint commands (fetching temps, setting target temps, pausing/resuming jobs, etc.).

## Environment Variables
To keep API keys out of version control, the Python scripts pull the OctoPrint API key directly from the Linux environment. Before running the compiled Matter app, the key must be exported in the terminal:

```bash
export OCTOPRINT_API_KEY="your_actual_key_here"
