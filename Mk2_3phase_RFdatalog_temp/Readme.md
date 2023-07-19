This program is to be used with the Arduino IDE and/or other development IDE lik

# Use with Arduino IDE

You'll need to download and install the **latest** [Arduino IDE](https://www.arduino.cc/en/software).
Since the code is optimized with the latest standard of C++, you'll need to edit a config file to activate C++17. 	

Please search the file '**platform.txt**' located in the installation path of the Arduino IDE. Typically, you'll find the file in '**C:\Program Files (x86)\Arduino\hardware\arduino\avr**' or in '**%LOCALAPPDATA%\Arduino15\packages\arduino\hardware\avr\x.y.z**' where 'x.y.z' is the version of the **Arduino AVR Boards** package.

Edit the file in any Text Editor (you'll need Admin rights) and replace the parameter '**-std=gnu++11**' with '**-std=gnu++17**'. That's it!	

If your Arduino IDE was opened, please close all the instances and open it again.	

# Use with Visual Studio Code

You'll need to install additional extension(s). The most popular and used extensions for this job are '*Arduino*' and '*Platform IO*'.

# Quick overview of the files

- **Mk2_3phase_RFdatalog_temp.ino** : This file is needed for Arduino IDE
- **calibration.h** : contains the calibration parameters
- **config.h** : the user's preferences are stored here (pin assignments, features, ...)
- **config_system.h** : rarely modified system constants
- **constants.h** : some constants - *do not edit*
- **debug.h** : some macros for serial output and debugging
- **dualtariff.h** : definitions for the dual tariff feature
- **main.cpp** : source code
- **main.h** : functions prototypes
- **movingAvg.h** : source code for sliding-window average
- **processing.cpp** : source code for the processing engine
- **processing.h** : functions prototype of the processing engine
- **Readme.md** : this file
- **types.h** : definitions of types, ...
- **type_traits.h** : some STL stuff not yet available in the avr-package
- **utils_relay.h** : source code for the *relay-diversion* feature
- **utils_rf.h** : source code for the *RF* feature
- **utils_temp.h** : source code for the *temperature* feature
- **utils.h** : helper functions and misc stuff
- **validation.h** : config validation, this code is executed during compile-time only !
- **platformio.ini** : PlatformIO configuration
- **inject_sketch_name.py** : helper script for PlatformIO
- **Doxyfile** : config for Doxygen (code documentation)

The end-user should ONLY edit both files **calibration.h** and **config.h**.
