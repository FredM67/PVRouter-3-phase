# A2 (pin 16) is an analog input (V2): no digital feature can use it. Up to now the pin check
# had a 16-bit mask, and a pin from 16 on passed.
temperature_on
replace 'temperatureSensing{ 3,' 'temperatureSensing{ 16,'
expect='Pins 14 and above'
