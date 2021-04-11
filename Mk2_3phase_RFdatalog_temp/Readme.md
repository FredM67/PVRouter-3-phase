This program is to be used with the Arduino IDE and/or other development IDE like Visual Studio Code.

# Use with Arduino IDE

You'll need to download and install the latest [Arduino IDE](https://www.arduino.cc/en/software).
Since the code is optimized with the latest standard of C++, you'll need to edit a config file to activate C++17. 

Please search the file '*platform.txt*' located in the installation path of the Arduino IDE. Typically, you'll find the file in '*C:\Program Files (x86)\Arduino\hardware\arduino\avr*'.

Edit the file in any Text Editor (you'll need Admin rights) and replace the parameter '**-std=gnu++11**' with '**-std=gnu++17**'. That's it!

If your Arduino IDE was opened, please close all the instances and open it again.

# Use with Visual Studio Code

You'll need to install addional extension(s). The most popular and used extensions for this job are '*Arduino*' and '*Platform IO*'.