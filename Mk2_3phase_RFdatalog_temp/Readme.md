[![en](https://img.shields.io/badge/lang-en-red.svg)](Readme.en.md)

Ce programme doit être utilisé avec l’IDE Arduino et/ou d’autres IDE de développement comme VSCode + PlatformIO.

- [Utilisation avec Arduino IDE](#utilisation-avec-arduino-ide)
- [Use with Visual Studio Code](#use-with-visual-studio-code)
- [Quick overview of the files](#quick-overview-of-the-files)
- [Étalonnage du routeur](#étalonnage-du-routeur)
- [Configuration du programme](#configuration-du-programme)
  - [Configuration des sorties TRIAC](#configuration-des-sorties-triac)
  - [Configuration des sorties relais tout-ou-rien](#configuration-des-sorties-relais-tout-ou-rien)
    - [Principe de fonctionnement](#principe-de-fonctionnement)
    - [Diagramme de fonctionnement](#diagramme-de-fonctionnement)
  - [Configuration du Watchdog](#configuration-du-watchdog)
  - [Configuration du ou des capteurs de température](#configuration-du-ou-des-capteurs-de-température)
    - [Avec l'Arduino IDE](#avec-larduino-ide)
    - [Avec Visual Studio Code et PlatformIO](#avec-visual-studio-code-et-platformio)
    - [Configuration du ou des capteurs (commun aux 2 cas précédents)](#configuration-du-ou-des-capteurs-commun-aux-2-cas-précédents)
  - [Configuration de la gestion des Heures Creuses (dual tariff)](#configuration-de-la-gestion-des-heures-creuses-dual-tariff)
    - [Configuration matérielle](#configuration-matérielle)
    - [Configuration logicielle](#configuration-logicielle)

# Utilisation avec Arduino IDE

Vous devrez télécharger et installer la version la plus récente de l'[Arduino IDE](https://www.arduino.cc/en/software).

Download the "standalone" version, NOT the version from the Microsoft Store.
Pick-up the "Win 10 and newer, 64 bits" or the "MSI installer" version.

Since the code is optimized with the latest standard of C++, you'll need to edit a config file to activate C++17. 	

Please search the file '**platform.txt**' located in the installation path of the Arduino IDE.

For **Windows**, typically, you'll find the file in '**C:\Program Files (x86)\Arduino\hardware\arduino\avr**' and/or in '**%LOCALAPPDATA%\Arduino15\packages\arduino\hardware\avr\x.y.z**' where 'x.y.z' is the version of the **Arduino AVR Boards** package.

You can type this command in a Powershell : `Get-Childitem –Path C:\ -Include platform.txt -Recurse -ErrorAction SilentlyContinue`. It could take a couple of seconds/minutes until the file is found.

For **Linux**, if using the AppImage package, you'll find this file in '**~/.arduino15/packages/arduino/hardware/avr/1.8.6**'.  
You can run `find / -name platform.txt 2>/dev/null` in case the location has been changed.

Edit the file in any Text Editor (you'll need **Admin rights**) and replace the parameter '**-std=gnu++11**' with '**-std=gnu++17**'. That's it!	

If your Arduino IDE was opened, please close all the instances and open it again.	

# Use with Visual Studio Code

You'll need to install additional extension(s). The most popular and used extensions for this job are '*Arduino*' and '*Platform IO*'.

# Quick overview of the files

- **Mk2_3phase_RFdatalog_temp.ino** : Ce fichier est nécessaire pour l’IDE Arduino
- **calibration.h** : contient les paramètres d’étalonnage
- **config.h** : les préférences de l’utilisateur sont stockées ici (affectation des broches, fonctionnalités, …)
- **config_system.h** : constantes système rarement modifiées
- **constants.h** : quelques constantes - *ne pas modifier*
- **debug.h** : Quelques macros pour la sortie série et le débogage
- **dualtariff.h** : définitions de la fonction double tarif
- **main.cpp** : code source principal
- **main.h** : prototypes de fonctions
- **movingAvg.h** : code source pour la moyenne glissante
- **processing.cpp** : code source du moteur de traitement
- **processing.h** : prototypes de fonctions du moteur de traitement
- **Readme.fr.md** : ce fichier
- **types.h** : définitions des types, …
- **type_traits.h** : quelques trucs STL qui ne sont pas encore disponibles dans le paquet avr
- **type_traits** : contient des patrons STL manquants
- **utils_relay.h** : code source de la fonctionnalité *diversion par relais*
- **utils_rf.h** : code source de la fonction *RF*
- **utils_temp.h** : code source de la fonctionnalité *Température*
- **utils.h** : fonctions d’aide et trucs divers
- **validation.h** : validation des paramètres, ce code n’est exécuté qu’au moment de la compilation !
- **platformio.ini** : paramètres PlatformIO
- **inject_sketch_name.py** : script d'aide pour PlatformIO
- **Doxyfile** : paramètre pour Doxygen (documentation du code)

L’utilisateur final ne doit éditer QUE les fichiers **calibration.h** et **config.h**.

# Étalonnage du routeur
Les valeurs d'étalonnage se trouvent dans le fichier **calibration.h**.
Il s'agit de la ligne :
```cpp
inline constexpr float f_powerCal[NO_OF_PHASES]{ 0.05000F, 0.05000F, 0.05000F };
```

Ces valeurs par défaut doivent être déterminées pour assurer un fonctionnement optimal du routeur.

# Configuration du programme

D'une manière générale, la configuration d'une fonctionnalité nécessite 2 changements au moins :
- activation de la fonctionnalité en question
- configuration de la fonctionnalité en question

La pertinence de l'ensemble est validée lors de la compilation. Ainsi, si par mégarde, une *pin* est allouée 2 fois par exemple, le compilateur émettra une erreur.

## Configuration des sorties TRIAC
Il faudra dans un 1<sup>er</sup> temps définir le nombre de sorties TRIAC.
```cpp
inline constexpr uint8_t NO_OF_DUMPLOADS{ 2 };
```

Ensuite, il faudra assigner les *pins* correspondantes ainsi que l'ordre des priorités au démarrage.
```cpp
inline constexpr uint8_t physicalLoadPin[NO_OF_DUMPLOADS]{ 5, 7 };
inline constexpr uint8_t loadPrioritiesAtStartup[NO_OF_DUMPLOADS]{ 0, 1 };
```

## Configuration des sorties relais tout-ou-rien
Les sorties relais tout-ou-rien permettent d'alimenter des appareils qui contiennent de l'électronique (pompe à chaleur, …).

Pour chaque relais, il faut définir 5 paramètres :
- numéro de pin sur laquelle est branché le relais
- seuil de surplus avant mise en route (par défaut **1000 W**)
- seuil d'import avant arrêt (par défaut **200 W**)
- durée de fonctionnement minimale en minutes (par défaut **5 mn**)
- durée d'arrêt minimale en minutes (par défaut **5 mn**).

```cpp
inline constexpr RelayEngine relays{ { { 4, 1000, 200, 10, 10 } } };
```
Cette ligne définit ainsi un relais câblé sur la pin **4**, qui se déclenchera à partir de **1000 W**, et qui s'arrêtera à partir de **200 W** d'import et donc le temps de fonctionnement mais aussi d'arrêt sera de **10 mn**.

Si plusieurs relais sont présents, on listera tout simplement les configurations de chaque relais de cette façon :
```cpp
inline constexpr RelayEngine relays{ { { 4, 1000, 200, 10, 10 },
                                       { 3, 1500, 250, 5, 15 } } };
```

### Principe de fonctionnement
Les valeurs de surplus ainsi que d'import sont calculées selon une moyenne mobile pondérée exponentiellement (EWMA pour Exponentially Weighted Moving Average).  
Par défaut, cette moyenne prend en compte une fenêtre d'environ 10 mn.  
Il est possible de la rallonger mais aussi de la raccourcir.  
Pour des raisons de performances de l'Arduino, la durée choisie sera arrondie à une durée proche qui permettra de faire les calculs sans impacter les performances du routeur.

Si l'utilisateur souhaite plutôt une fenêtre de 15 mn, il suffira d'écrire :
```cpp
inline constexpr RelayEngine relays{ 15_i, { { 3, 1000, 200, 1, 1 } } };
```
___
**_Note_**
Attention au suffixe '**_i**' après le nombre *15* !
___

### Diagramme de fonctionnement
À venir...

## Configuration du Watchdog
Un chien de garde, en anglais *watchdog*, est un circuit électronique ou un logiciel utilisé en électronique numérique pour s'assurer qu'un automate ou un ordinateur ne reste pas bloqué à une étape particulière du traitement qu'il effectue.

Ceci est réalisé à l'aide d'une LED qui clignote à la fréquence de 1 Hz, soit toutes les secondes.  
Ainsi, l'utilisateur sait d'une part si son routeur est allumé, et si jamais cette LED ne clignote plus, c'est que l'Arduino s'est bloqué (cas encore jamais rencontré !).  
Un simple appui sur le bouton *Reset* permettra de redémarrage le système sans rien débrancher.

Il faudra activer la fonctionnalité comme ceci :
```cpp
inline constexpr bool WATCHDOG_PIN_PRESENT{ true };
```
et définir la *pin* utilisée, dans l'exemple la *9* :
```cpp
inline constexpr uint8_t watchDogPin{ 9 };
```

## Configuration du ou des capteurs de température
Il est possible de brancher un ou plusieurs capteurs de température Dallas DS18B20.  
Ces capteurs peuvent être utilisés de façon purement informative mais aussi pour le contrôle de la marche forcée.

Pour activer cette fonctionnalité, il faudra procéder différemment selon que l'on utilise l'Arduino IDE ou Visual Studio Code avec l'extension PlatformIO.

### Avec l'Arduino IDE
If faudra activer la ligne :
```cpp
//#define TEMP_ENABLED
```
en supprimant le commentaire, comme ceci :
```cpp
#define TEMP_ENABLED
```

Si la bibliothèque *OneWire* n'est pas encore installée, il faudra procéder à son installation :
- Menu **Outils**=>**Gérer les bibliothèques...**
- Taper "Onewire" dans le champ de recherche
- Installer "**OneWire** par Jim Studt, ..." en version **2.3.7** ou plus récente.

### Avec Visual Studio Code et PlatformIO
Dans ce cas, il faudra sélectionner la configuration "**env:temperature (Mk2_3phase_RFdatalog_temp)**".

### Configuration du ou des capteurs (commun aux 2 cas précédents)
Pour configurer le ou les capteurs, il faudra saisir leur·s adresse·s.  
Pour cela, il faudra utiliser un programme qui permettra de scanner les capteurs connectés. Ce genre de programme est disponible un peu partout sur Internet mais aussi parmi les croquis d'exemple fournis avec l'Arduino IDE.  
Il est conseillé de noter l'adresse de chaque capteur sur une étiquette adhésive que l'on collera sur le câble du capteur correspondant.

Les adresses seront alors saisies de la façon suivante :
```cpp
inline constexpr TemperatureSensing temperatureSensing{ 4,
                                                        { { 0x28, 0xBE, 0x41, 0x6B, 0x09, 0x00, 0x00, 0xA4 },
                                                          { 0x28, 0x1B, 0xD7, 0x6A, 0x09, 0x00, 0x00, 0xB7 } } };
```
Le nombre *4* en 1<sup>er</sup> paramètre est la *pin* que l'utilisateur aura choisi pour le bus OneWire.

___
**_Note_**
Plusieurs capteurs peuvent être branchés sur le même câble.  
Sur Internet vous trouverez tous les détails concernant la topologie utilisable avec ce genre de capteurs.
___

## Configuration de la gestion des Heures Creuses (dual tariff)
Il est possible de confier la gestion des Heures Creuses par le routeur.  
Cela permet par exemple de limiter la chauffe en marche forcée afin de ne pas trop chauffer l'eau dans l'optique d'utiliser le surplus le lendemain matin.  
Cette limite peut être en durée ou en température (nécessite d'utiliser un capteur de température Dallas DS18B20).

### Configuration matérielle
Il faudra décâbler la commande du contacteur Jour/Nuit, il ne servira plus à rien.  
Ensuite, il conviendra de relier *directement* une *pin* choisie au relais incorporé dans le compteur (bornes C1 et C2).
___
**__ATTENTION__**
Il faut relier **directement**, une paire *pin/masse* avec les bornes *C1/C2* du compteur.  
Il NE doit PAS y avoir de 230V sur ce circuit !
___

### Configuration logicielle
Cette fonctionnalité s'active via la ligne :
```cpp
inline constexpr bool DUAL_TARIFF{ true };
```
Il faudra aussi choisir le *pin* sur laquelle est relié le compteur :
```cpp
inline constexpr uint8_t dualTariffPin{ 3 };
```

Il faudra aussi la durée en *heures* de la période d'Heures Creuses (pour l'instant, une seule période est supportée par jour)  :
```cpp
inline constexpr uint8_t ul_OFF_PEAK_DURATION{ 8 };
```

Enfin, on définira les modalités de fonctionnement pendant la période d'Heures Creuses :
```cpp
inline constexpr pairForceLoad rg_ForceLoad[NO_OF_DUMPLOADS]{ { -3, 2 } };
```
Il est possible de définir une configuration pour chaque charge indépendamment l'une des autres.
Le 1<sup>er</sup> paramètre détermine la temporisation de démarrage par rapport au début de la période d'Heures Creuses ou la fin de cette période  :
- si le nombre est positif et inférieur à 24, il s'agit du nombre d'heures,
- si le nombre est négatif supérieur à -24, il s'agit du nombre d'heures par rapport à la fin des Heures Creuses
- si le nombre est positif et supérieur à 24, il s'agit du nombre de minutes,
- si le nombre est négatif inférieur à -24, il s'agit du nombre de minutes par rapport à la fin des Heures Creuses

Le 2<sup>ème</sup> paramètre détermine la durée de la marche forcée :
- si le nombre est inférieur à 24, il s'agit du nombre d'heures,
- si le nombre est supérieur à 24, il s'agit du nombre de minutes.

Prenons quelques exemples pour mieux comprendre (avec début d'HC à 23:00, jusqu'à 7:00 soit 8 h de durée) :
- ```{ -3, 2 }``` signifie démarrage **3 heures AVANT** la fin de période (à 4 h du matin), pour une durée de 2 h.
- ```{ 3, 2 }``` signifie démarrage **3 heures APRÈS** la début de période (à 2 h du matin), pour une durée de 2 h.
- ```{ -150, 2 }``` signifie démarrage **150 minutes AVANT** la fin de période (à 4:30), pour une durée de 2 h.
- ```{ 3, 180 }``` signifie démarrage **3 heures APRÈS** la début de période (à 2 h du matin), pour une durée de 180 mn.

Dans le cas où l'on désire une durée *infinie* (donc jusqu'à la fin de la période d'HC), il faudra écrire par exemple :
- ```{ -3, UINT16_MAX }``` signifie démarrage **3 heures AVANT** la fin de période (à 4 h du matin) avec marche forcée jusqu'à la fin de période d'HC.

Dans un système comprenant 2 sorties (```NO_OF_DUMPLOADS``` aura alors une valeur de 2), si l'on souhaite une marche forcée uniquement sur la 2<sup>ème</sup> sortie, on écrira :
```cpp
inline constexpr pairForceLoad rg_ForceLoad[NO_OF_DUMPLOADS]{ { 0, 0 },
                                                              { -3, 2 } };
```

*doc non finie*