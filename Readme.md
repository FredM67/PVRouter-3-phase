<div align = center>

[![GitHub issues](https://img.shields.io/github/issues/FredM67/PVRouter-3-phase)](https://github.com/FredM67/PVRouter-3-phase/issues)
[![GitHub forks](https://img.shields.io/github/forks/FredM67/PVRouter-3-phase)](https://github.com/FredM67/PVRouter-3-phase/network)
[![GitHub stars](https://img.shields.io/github/stars/FredM67/PVRouter-3-phase)](https://github.com/FredM67/PVRouter-3-phase/stargazers)
[![CodeQL](https://github.com/FredM67/PVRouter-3-phase/actions/workflows/codeql.yml/badge.svg)](https://github.com/FredM67/PVRouter-3-phase/actions/workflows/codeql.yml)
[![Doxygen](https://github.com/FredM67/PVRouter-3-phase/actions/workflows/doxygen-gh-pages.yml/badge.svg)](https://github.com/FredM67/PVRouter-3-phase/actions/workflows/doxygen-gh-pages.yml)
<br/>
[![Stand With Ukraine](https://raw.githubusercontent.com/vshymanskyy/StandWithUkraine/main/badges/StandWithUkraine.svg)](https://stand-with-ukraine.pp.ua)
<br/>
<br/>
  [![en](https://img.shields.io/badge/lang-en-red.svg)](Readme.en.md)
  [![fr](https://img.shields.io/badge/lang-fr-blue.svg)](Readme.md)
</div>

# PVRouter (version triphasée)

My version of the 3-phase Mk2PVRouter firmware (see <http://www.mk2pvrouter.co.uk>).

Robin Emley propose déjà un routeur PV triphasé (https://www.mk2pvrouter.co.uk/3-phase-version.html).  
Il prend en charge jusqu'à 12 sorties pour charges résistives, qui sont complètement indépendantes.

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
  - [Support](#support)
  - [Roadmap](#roadmap)
  - [Contributing](#contributing)
  - [Authors and acknowledgment](#authors-and-acknowledgment)

## Photo Gallery

[Here](Gallery.md) a couple of pictures of assembled routers.

## Schematic of the mainboard

[Here](../../schematics/3phase_Mainboard.pdf) the schematic of the mainboard.

## Documentation de développement

Vous pouvez commencer à lire la documentation ici [3-phase routeur](https://fredm67.github.io/PVRouter-3-phase/) (en anglais).

## Documentation de l’utilisateur final

### Aperçu

L’objectif était de modifier/optimiser le programme pour le cas « spécial » d’un chauffe-eau triphasé. Un chauffe-eau triphasé est composé en fait de 3 éléments de chauffage indépendants. La plupart du temps, un tel chauffe-eau peut être connecté en monophasé, en triphasé étoile (WYE) ou triphasé triangle (Delta). Lorsqu’il est connecté en étoile, il n’y a pas besoin de fil de neutre parce que le système est équilibré, donc à tout moment, il n’y a pas de courant qui circule vers le neutre.

Fonctionnalités ajoutées :

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

### Capteur de température

Il peut être utilisé pour optimiser le fonctionnement de la marche forcée, pour prendre la bonne décision pendant la nuit.

### Enphase zero-export profile

When zero-export settings is enabled, the PV system curtails power production if the production of the system exceeds the consumption needs of the site. This ensures zero feed into the grid.

Comme effet secondaire, le routeur ne verra pas à aucun moment un surplus d’énergie.  
L’idée est donc d’appliquer un certain décalage à l’énergie mesurée par le routeur.
Comme il est déjà commenté dans le code, après l'assignation d’une valeur négative à *REQUIRED_EXPORT_IN_WATTS*, le routeur agira comme un générateur PV.  
Si vous définissez une valeur de *-20*, chaque fois que le routeur mesure le flux d’énergie, il ajoutera *-20* aux mesures.  

So, now let see what happen in a couple of cases:

- la valeur mesurée est **positive** (importation d’énergie = pas d’excédent), après avoir ajouté *-20*, cela reste positif, le routeur ne fait rien. Pour une valeur comprise entre -20 et 0, le déviateur ne fera rien non plus.
- la valeur mesurée est **autour de zéro**. Dans cette situation, la limitation du "profil zéro exportation" est active.  
Après l’ajout de *-20*, nous obtenons une valeur négative, ce qui déclenchera le détournement d’énergie vers le chauffe-eau.  
Ensuite, il y a une sorte de réaction en chaîne. L’Envoy détecte plus de consommation, décide d’augmenter la production.  
À la mesure suivante, le routeur mesure à nouveau une valeur autour de zéro, ajoute à nouveau -20, et détourne encore plus d’énergie.  
Lorsque la production (et l’excédent) arrive au maximum possible, la valeur mesurée restera autour de zéro+ et le système deviendra stable.

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

![Heater with mechanical thermostat](img/Heater_mechanical.png)  
*Figure: Wiring diagram*

## Heater with ACI single phase thermostat

In this case, it's somehow the same situation as before.
You don't need to buy a 3-phase kit to convert your single phase heater.
The ACI pcb must be connected to a permanent phase. It will then control any 3-phase relay.

![Heater with ACI single phase thermostat](img/Heater_ACI_Mono.png)  
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

![Heater with ACI 3-phase thermostat](img/Heater_ACI_Tri.png)  
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

![Mechanical thermostat](img/Thermostat.png)  
*Figure: An example of a thermostat*

---

![Heater with mechanical thermostat](img/Heater_mechanical-No_neutral.png)  
*Figure: Wiring diagram*

## Support

This project is maintained by [@FredM67](https://github.com/FredM67). Please understand that we won't be able to provide individual support via email. We also believe that help is much more valuable if it's shared publicly, so that more people can benefit from it.

| Type                                  | Platforms                                                                     |
| ------------------------------------- | ----------------------------------------------------------------------------- |
| 🚨 **Bug Reports**                     | [GitHub Issue Tracker](https://github.com/FredM67/PVRouter-3-phase/issues)    |
| 📚 **Docs Issue**                      | [GitHub Issue Tracker](https://github.com/FredM67/PVRouter-3-phase/issues)    |
| 🎁 **Feature Requests**                | [GitHub Issue Tracker](https://github.com/FredM67/PVRouter-3-phase/issues)    |
| 🛡 **Report a security vulnerability** | See [SECURITY.md](SECURITY.md)                                                |
| 💬 **General Questions**               | [GitHub Discussions](https://github.com/FredM67/PVRouter-3-phase/discussions) |

## Roadmap

No changes are currently planned.

## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md) for details on our code of conduct, and the process for submitting pull requests to us.

## Authors and acknowledgment

- **Fr�d�ric Metrich** - _Initial work_ - [FredM67](https://github.com/FredM67)

See also the list of [contributors](https://github.com/FredM67/PVRouter-3-phase/graphs/contributors) who participated in this project.
