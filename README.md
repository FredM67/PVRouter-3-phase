# mk2pvrouter
My version of the 3-phase Mk2PVRouter firmware (see http://www.mk2pvrouter.co.uk)

Robin Emley already proposes a 3 phase PV-router (https://www.mk2pvrouter.co.uk/3-phase-version.html).
It supports 3 resistive output loads, which a completely independant.

Goal was to modify/optimize the sketch for the "special" case of a 3-phase water heater. A 3-phase water heater is composed in fact of 3 independant heating elements. Most of the time, such a heater can be connected in mono, or 3-phase WYE or 3-phase Delta.
When connected in WYE (without variator), there's no need of a neutral wire because the system is equaly distributed, so at any time, there's no current flowing to the neutral.
If a variator is used, the neutral wire must be connected.

Added functionalities:
- load priorities management
- off-peak period detection
- force full power
- temperature sensor (just reading for the moment)
- optimized (RF) data logging

The original sketch had to be completely re-worked and re-structured to support temperature reading. In the original sketch, the ISR "just" reads and converts the analog data, and the processing is done in the loop. This won't work with a temperatur sensor because of its slow performance. It would break the whole system, current/voltage data will be lost, ...

Now, all the time-critical processing is done inside the ISR, other stuff like (RF) data logging, Serial printing, temperature reading is made inside the loop(). The ISR and main processor communicate with each other through "events".

### load priorities management
In my variant of Robin's sketch, the 3 loads are still physically independant, so it means, the router will divert surplus of energy to the first load (highest priority) from 0% to 100%, then to the second (0% to 100%) and finally to the third.
To avoid that the priorities stays all the time unchanged, which would mean that load 1 will run much more than load 2, which again will run much more than 3, I add a priority management.
Each day, the load priorities are rotated, so over many days, all the heating elements will run somehow the same amount of time.

### off-peak period detection
Depending on the country, some energy meters provide a switch which toggle on at the beggining of the off-peak period. It is intended to control a relay. If you wire it to a free digital pin of the router (in my case D3), you can detect off-peak/peak period.

### force full power
Support has been added to force full power on all loads. In my variant, that's used to switch the heater one during off-peak period if not enough surplus has been routed during the day. Here, to optimize the behaviour, a temp-sensor will be used to check the temperature of the water and decide to switch on or not during night.

### temperature sensor
For the moment, just reading. It'll be used to optimize force full power, to make the right decision during might.
