<p align="center">
  <a href="https://github.com/FredM67/PVRouter-3-phase/">Français</a> |
  <span>English</span>
</p>

# PVRouter (3-phase version)

My version of the 3-phase Mk2PVRouter firmware (see <http://www.mk2pvrouter.co.uk>).

Robin Emley already proposes a 3 phase PV-router (<https://www.mk2pvrouter.co.uk/3-phase-version.html>).
It supports up to 7 resistive output loads, which are completely independent.

---
**_NOTE:_** For a single phase version, please see [PVRouter-Single](https://github.com/FredM67/PVRouter-Single).

---

- [PVRouter (3-phase version)](#pvrouter-3-phase-version)
  - [Photo Gallery](#photo-gallery)
  - [Schematic of the mainboard](#schematic-of-the-mainboard)
  - [Implementation documentation](#implementation-documentation)
  - [End-user documentation](#end-user-documentation)
    - [Overview](#overview)
    - [Load priorities management](#load-priorities-management)
    - [Off-peak period detection](#off-peak-period-detection)
    - [Force full power](#force-full-power)
    - [Temperature sensor](#temperature-sensor)
    - [Enphase zero-export profile](#enphase-zero-export-profile)
- [How to wire the router](#how-to-wire-the-router)
- [Use cases](#use-cases)
  - [Requirements](#requirements)
  - [Heater with mechanical thermostat](#heater-with-mechanical-thermostat)
      - [Migrate from single-phase to 3-phase (with neutral wire)](#migrate-from-single-phase-to-3-phase-with-neutral-wire)
      - [Wiring](#wiring)
  - [Heater with ACI single phase thermostat](#heater-with-aci-single-phase-thermostat)
  - [Heater with ACI 3-phase thermostat (without neutral wire)](#heater-with-aci-3-phase-thermostat-without-neutral-wire)
  - [Alternatives WITHOUT neutral wire](#alternatives-without-neutral-wire)
    - [Heater with mechanical thermostat](#heater-with-mechanical-thermostat-1)

## Photo Gallery

[Here](Gallery.md) a couple of pictures of assembled routers.

## Schematic of the mainboard

[Here](../../schematics/3phase_Mainboard.pdf) the schematic of the mainboard.

## Implementation documentation

You can start reading the documentation here [3-phase diverter](https://fredm67.github.io/PVRouter-3-phase/html/index.html).

## End-user documentation

### Overview

Goal was to modify/optimize the sketch for the "special" case of a 3-phase water heater. A 3-phase water heater is composed in fact of 3 independent heating elements. Most of the time, such a heater can be connected in mono, or 3-phase WYE or 3-phase Delta.
When connected in WYE (without varistor), there's no need of a neutral wire because the system is equally distributed, so at any time, there's no current flowing to the neutral.

If a diverter is used, the neutral wire must be connected.

Added functionalities:

- load priorities management (configurable)
- off-peak period detection (configurable)
- force full power
- temperature sensor (just reading for the moment)
- optimized (RF) data logging
- serial output in JSON or TXT

The original sketch had to be completely re-worked and re-structured to support temperature reading. In the original sketch, the ISR "just" reads and converts the analog data, and the processing is done in the loop. This won't work with a temperature sensor because of its slow performance. It would break the whole system, current/voltage data will be lost, ...

Now, all the time-critical processing is done inside the ISR, other stuff like (RF) data logging, Serial printing, temperature reading is made inside the loop(). The ISR and main processor communicate with each other through "events".

### Load priorities management

In my variant of Robin's sketch, the 3 loads are still physically independent, so it means, the router will divert surplus of energy to the first load (highest priority) from 0% to 100%, then to the second (0% to 100%) and finally to the third.

To avoid that the priorities stay all the time unchanged, which would mean that load 1 will run much more than load 2, which again will run much more than 3, I've added a priority management.
Each day, the load priorities are rotated, so over many days, all the heating elements will run somehow the same amount of time.

### Off-peak period detection

Depending on the country, some energy meters provide a switch/relay which toggles on at the beginning of the off-peak period. It is intended to control a relay. If you wire it to a free digital pin of the router (in my case D3), you can detect off-peak/peak period.

### Force full power

Support has been added to force full power on specific loads. Each load can be forced independently from each other, start time and duration can be set individually.

In my variant, that's used to switch the heater one during off-peak period if not enough surplus has been routed during the day. Here, to optimize the behavior, a temp-sensor will be used to check the temperature of the water and decide to switch on or not during night.

### Temperature sensor

For the moment, just reading. It'll be used to optimize force full power, to make the right decision during night.

### Enphase zero-export profile

When zero-export settings is enabled, the PV system curtails power production if the production of the system exceeds the consumption needs of the site. This ensures zero feed into the grid.

As a side effect, the diverter won't see at any time surplus of energy.  
So the idea is to apply a certain offset to the energy measured by the diverter.
As it is already commented in the code, after setting a negative value to *REQUIRED_EXPORT_IN_WATTS*, the diverter will act as a PV generator.  
If you set a value of -20, each time the diverter measures the energy flowing, it'll add *-20* to the measurements.  

So, now let see what happen in a couple of cases:

- measured value is **positive** (energy import = no surplus), after adding *-20*, it stays positive, the diverter doesn't do anything. By a value between -20 and 0, the diverter won't do anything either.
- measured value is **around zero**. In this situation, the "zero export profile" limitation is active.  
After adding *-20*, we get a negative value that will make the diverter start diverting energy to the water heater.  
Now, there's a sort of chain reaction. The Envoy detects more consumption, decides to raise production.  
On the next measurement, the diverter measures again a value around zero, add again *-20*, and diverts even more energy.  
When production (and surplus) gets to the maximum possible, the measured value will stay around zero+ and the system is stable.

This has been tested in real by Amorim. Depending of each situation, it might be necessary to tweak this value of *-20* to a bigger or smaller value.

# How to wire the router
[Here](../../docs/HowToInstall.pdf) you'll find a quick how-to for installing/wiring your router.

# Use cases

I want to:

- change my (mechanical) single-phase water heater to 3-phase, see [Heater with mechanical thermostat](#heater-with-mechanical-thermostat)
- connect my (mechanical) 3-phase water heater, see [Heater with mechanical thermostat](#heater-with-mechanical-thermostat)
- change my ACI single-phase water heater to 3-phase w/o buying a 3-phase kit, see [Heater with ACI single phase thermostat](#heater-with-aci-single-phase-thermostat)
- connect my ACI 3-phase water heater, see [Heater with ACI 3-phase thermostat (without neutral wire)](#heater-with-aci-3-phase-thermostat-without-neutral-wire)
- connect multiple pure resistive charges, simply wire them, one on each output, and do not forget to disable [Load priorities management](#load-priorities-management).

## Requirements

To change your single-phase water heater to 3-phase, it MUST support 3-phase wiring (i.e. it must have 3 heating elements).

---
**_Safety Warning_**

To modify the existing wiring, access to 240V mains voltage is required.  
Please take great care, and do not undertake this stage unless you feel confident to do so.

---

## Heater with mechanical thermostat

#### Migrate from single-phase to 3-phase (with neutral wire)

---
**_A router with 3 outputs is needed_**

With this solution, you'll control each heating element separately.

---

You'll have to separate all 3 heating elements, and probably add a new wire for each of them. Sometime, the elements are connected together with a sort of metallic "star". There's one for the (single) phase, and one for the neutral wire. You only need to remove one of them, the one for neutral must stay wired.  

#### Wiring

Since on all (3-phase) water heaters I've seen, the thermostat switches only 2 phases in normal mode (all 3 phases in security mode), it must be wired in another way to achieve a full switch on all 3 phases. In a fully balanced 3-phase situation, you don't need any neutral wire. To switch off the device, you only need to switch off 2 phases.

---
**_Note_**

In a balanced situation, you don't need any neutral wire. To switch off the device, you just need to switch off 2 phases out of 3. That's why most thermostats are build like this.

---

For that, I've "recycled" a peak/off peak 3-phase relay but you can use any 3-phase relay. It doesn't matter on which phase the command coil is connected, but it must be permanent (not through the router).

![Heater with mechanical thermostat](../../img/Heater_mechanical.png)  
*Figure: Wiring diagram*

## Heater with ACI single phase thermostat

In this case, it's somehow the same situation as before.
You don't need to buy a 3-phase kit to convert your single phase heater.
The ACI pcb must be connected to a permanent phase. It will then control any 3-phase relay.

![Heater with ACI single phase thermostat](../../img/Heater_ACI_Mono.png)  
*Figure: Wiring diagram*

## Heater with ACI 3-phase thermostat (without neutral wire)

---
**_A router with 2 outputs is needed_**

With this solution, you'll control each heating element separately.

---

The ACI board does not cut all 3 phases when the temperature is reached. Only 2 phases are disconnected.

The remaining connected phase is the one in the middle of the power connector.
***It is very IMPORTANT that this phase, which remains permanent, does not pass through a triac***.

The ACI pcb must be connected to 3 permanent phases.

![Heater with ACI 3-phase thermostat](../../img/Heater_ACI_Tri.png)  
*Figure: Wiring diagram*

## Alternatives WITHOUT neutral wire

---
**_A router with 2 outputs is needed_**

With this solution, you won't need to add an additional neutral wire nor add a relay.

---

### Heater with mechanical thermostat

This configuration allows to simplify the wiring and specially does not require any 3-4 poles relay.

---
**_Zoom on the thermostat_**

You need to take care of which wires are switched off.

In **red**, security switch (see the 'S' on each pole) : all 3 phases are switched off.

In **green**, only 2 phases are switched off, L2 et L3. ***It is IMPORTANT that the phase L1, not switched by the thermostat, DOES NOT pass through the triac***.

![Mechanical thermostat](../../img/Thermostat.png)  
*Figure: An example of a thermostat*

---

![Heater with mechanical thermostat](../../img/Heater_mechanical-No_neutral.png)  
*Figure: Wiring diagram*
