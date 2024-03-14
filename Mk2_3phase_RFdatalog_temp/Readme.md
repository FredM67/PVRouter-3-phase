[![en](https://img.shields.io/badge/lang-en-red.svg)](Readme.en.md)

Ce programme est conçu pour être utilisé avec l'IDE Arduino et/ou d'autres IDE de développement comme VSCode + PlatformIO.

- [Utilisation avec Arduino IDE](#utilisation-avec-arduino-ide)
- [Utilisation avec Visual Studio Code](#utilisation-avec-visual-studio-code)
- [Aperçu rapide des fichiers](#aperçu-rapide-des-fichiers)
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
  - [Rotation des priorités](#rotation-des-priorités)
  - [Configuration de la marche forcée](#configuration-de-la-marche-forcée)
  - [Arrêt du routage](#arrêt-du-routage)

# Utilisation avec Arduino IDE

Pour utiliser ce programme avec l'IDE Arduino, vous devez télécharger et installer la dernière version de l'IDE Arduino. Choisissez la version "standard", PAS la version du Microsoft Store. Optez pour la version "Win 10 et plus récent, 64 bits" ou la version "MSI installer".

Comme le code est optimisé avec l'une des dernières normes C++, vous devez modifier un fichier de configuration pour activer C++17. Vous trouverez le fichier '**platform.txt**' dans le chemin d'installation de l'IDE Arduino.

Pour **Windows**, vous trouverez généralement le fichier dans '**C:\Program Files (x86)\Arduino\hardware\arduino\avr**' et/ou dans '**%LOCALAPPDATA%\Arduino15\packages\arduino\hardware\avr\x.y.z**' où **'x.y.z**' est la version du package Arduino AVR Boards.

Vous pouvez également exécuter cette commande dans Powershell : `Get-Childitem –Path C:\ -Include platform.txt -Recurse -ErrorAction SilentlyContinue`. Cela peut prendre quelques secondes/minutes jusqu'à ce que le fichier soit trouvé.

Pour **Linux**, si vous utilisez le package AppImage, vous trouverez ce fichier dans '~/.arduino15/packages/arduino/hardware/avr/1.8.6'. Vous pouvez exécuter `find / -name platform.txt 2>/dev/null` au cas où l'emplacement aurait changé.

Ouvrez le fichier dans n'importe quel éditeur de texte (vous aurez besoin des droits d'administrateur) et remplacez le paramètre '**-std=gnu++11**' par '**-std=gnu++17**'. C'est tout !

Si votre IDE Arduino était ouvert, veuillez fermer toutes les instances et le rouvrir.

# Utilisation avec Visual Studio Code

Vous devrez installer des extensions supplémentaires. Les extensions les plus populaires et les plus utilisées pour ce travail sont '*Arduino*' et '*Platform IO*'.  
L'ensemble du projet a été conçu pour être utilisé de façon optimale avec *Platform IO*.

# Aperçu rapide des fichiers

- **Mk2_3phase_RFdatalog_temp.ino** : Ce fichier est nécessaire pour l’IDE Arduino
- **calibration.h** : contient les paramètres d’étalonnage
- **config.h** : les préférences de l’utilisateur sont stockées ici (affectation des broches, fonctionnalités …)
- **config_system.h** : constantes système rarement modifiées
- **constants.h** : quelques constantes — *ne pas modifier*
- **debug.h** : Quelques macros pour la sortie série et le débogage
- **dualtariff.h** : définitions de la fonction double tarif
- **main.cpp** : code source principal
- **main.h** : prototypes de fonctions
- **movingAvg.h** : code source pour la moyenne glissante
- **processing.cpp** : code source du moteur de traitement
- **processing.h** : prototypes de fonctions du moteur de traitement
- **Readme.fr.md** : ce fichier
- **types.h** : définitions des types …
- **type_traits.h** : quelques trucs STL qui ne sont pas encore disponibles dans le paquet avr
- **type_traits** : contient des patrons STL manquants
- **utils_relay.h** : code source de la fonctionnalité *diversion par relais*
- **utils_rf.h** : code source de la fonction *RF*
- **utils_temp.h** : code source de la fonctionnalité *Température*
- **utils.h** : fonctions d’aide et trucs divers
- **validation.h** : validation des paramètres, ce code n’est exécuté qu’au moment de la compilation !
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
Il faudra dans un 1ᵉʳ temps définir le nombre de sorties TRIAC.
```cpp
inline constexpr uint8_t NO_OF_DUMPLOADS{ 2 };
```

Ensuite, il faudra assigner les *pins* correspondantes ainsi que l'ordre des priorités au démarrage.
```cpp
inline constexpr uint8_t physicalLoadPin[NO_OF_DUMPLOADS]{ 5, 7 };
inline constexpr uint8_t loadPrioritiesAtStartup[NO_OF_DUMPLOADS]{ 0, 1 };
```

## Configuration des sorties relais tout-ou-rien
Les sorties relais tout-ou-rien permettent d'alimenter des appareils qui contiennent de l'électronique (pompe à chaleur …).

Il faudra activer la fonctionalité comme ceci :
```cpp
inline constexpr bool RELAY_DIVERSION{ true };
```

Pour chaque relais, il faut définir 5 paramètres :
- numéro de **pin** sur laquelle est branché le relais
- **seuil de surplus** avant mise en route (par défaut **1000 W**)
- **seuil d'import** avant arrêt (par défaut **200 W**)
- **durée de fonctionnement minimale** en minutes (par défaut **5 min**)
- **durée d'arrêt minimale** en minutes (par défaut **5 min**).

Exemple :
```cpp
inline constexpr RelayEngine relays{ { { 4, 1000, 200, 10, 10 } } };
```
Cette ligne définit ainsi un relais câblé sur la *pin* **4**, qui se déclenchera à partir de **1000 W** de surplus, et qui s'arrêtera à partir de **200 W** d'import et dont le temps de fonctionnement mais aussi d'arrêt seront de **10 min**.

Si plusieurs relais sont présents, on listera tout simplement les configurations de chaque relais de cette façon :
```cpp
inline constexpr RelayEngine relays{ { { 4, 1000, 200, 10, 10 },
                                       { 3, 1500, 250, 5, 15 } } };
```
Les relais seront mis en route dans le même ordre que dans la liste. L'ordre d'arrêt sera l'inverse.  
Dans tous les cas, les consignes de durée de fonctionnement et d'arrêt seront respectées.

### Principe de fonctionnement
Les valeurs de surplus ainsi que d'import sont calculées selon une moyenne mobile pondérée exponentiellement (**EWMA** pour **E**xponentially **W**eighted **M**oving **A**verage).  
Par défaut, cette moyenne prend en compte une fenêtre d'environ 10 min.  
Il est possible de la rallonger mais aussi de la raccourcir.  
Pour des raisons de performances de l'Arduino, la durée choisie sera arrondie à une durée proche qui permettra de faire les calculs sans impacter les performances du routeur.

Si l'utilisateur souhaite plutôt une fenêtre de 15 min, il suffira d'écrire :
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
Le nombre *4* en 1ᵉʳ paramètre est la *pin* que l'utilisateur aura choisi pour le bus OneWire.

___
**_Note_**
Plusieurs capteurs peuvent être branchés sur le même câble.  
Sur Internet vous trouverez tous les détails concernant la topologie utilisable avec ce genre de capteurs.
___

## Configuration de la gestion des Heures Creuses (dual tariff)
Il est possible de confier la gestion des Heures Creuses au routeur.  
Cela permet par exemple de limiter la chauffe en marche forcée afin de ne pas trop chauffer l'eau dans l'optique d'utiliser le surplus le lendemain matin.  
Cette limite peut être en durée ou en température (nécessite d'utiliser un capteur de température Dallas DS18B20).

### Configuration matérielle
Il faudra décâbler la commande du contacteur Jour/Nuit, il ne servira plus à rien.  
Ensuite, il conviendra de relier *directement* une *pin* choisie au contact sec incorporé dans le compteur (bornes C1 et C2).
___
**__ATTENTION__**
Il faut relier **directement**, une paire *pin/masse* avec les bornes *C1/C2* du compteur.  
Il NE doit PAS y avoir de 230 V sur ce circuit !
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
Le 1ᵉʳ paramètre détermine la temporisation de démarrage par rapport au début de la période d'Heures Creuses ou la fin de cette période  :
- si le nombre est positif et inférieur à 24, il s'agit du nombre d'heures,
- si le nombre est négatif supérieur à −24, il s'agit du nombre d'heures par rapport à la fin des Heures Creuses
- si le nombre est positif et supérieur à 24, il s'agit du nombre de minutes,
- si le nombre est négatif inférieur à −24, il s'agit du nombre de minutes par rapport à la fin des Heures Creuses

Le 2ᵉ paramètre détermine la durée de la marche forcée :
- si le nombre est inférieur à 24, il s'agit du nombre d'heures,
- si le nombre est supérieur à 24, il s'agit du nombre de minutes.

Prenons quelques exemples pour mieux comprendre (avec début d'HC à 23:00, jusqu'à 7:00 soit 8 h de durée) :
- ```{ -3, 2 }``` signifie démarrage **3 heures AVANT** la fin de période (à 4 h du matin), pour une durée de 2 h.
- ```{ 3, 2 }``` signifie démarrage **3 heures APRÈS** la début de période (à 2 h du matin), pour une durée de 2 h.
- ```{ -150, 2 }``` signifie démarrage **150 minutes AVANT** la fin de période (à 4:30), pour une durée de 2 h.
- ```{ 3, 180 }``` signifie démarrage **3 heures APRÈS** la début de période (à 2 h du matin), pour une durée de 180 min.

Dans le cas où l'on désire une durée *infinie* (donc jusqu'à la fin de la période d'HC), il faudra écrire par exemple :
- ```{ -3, UINT16_MAX }``` signifie démarrage **3 heures AVANT** la fin de période (à 4 h du matin) avec marche forcée jusqu'à la fin de période d'HC.

Dans un système comprenant 2 sorties (```NO_OF_DUMPLOADS``` aura alors une valeur de 2), si l'on souhaite une marche forcée uniquement sur la 2ᵉ sortie, on écrira :
```cpp
inline constexpr pairForceLoad rg_ForceLoad[NO_OF_DUMPLOADS]{ { 0, 0 },
                                                              { -3, 2 } };
```

## Rotation des priorités
Lorsqu'on alimente un chauffe-eau triphasé, il peut être judicieux de permuter les priorités de mise en route de chaque résistance toutes les 24 h.  
Ainsi, en moyenne sur plusieurs semaines, chaque résistance aura fonctionné à peu près la même durée.  

Une fois n'est pas coutume, l'activation de cette fonction possède 2 modes :
- **automatique**, on spécifiera alors
```cpp
inline constexpr RotationModes PRIORITY_ROTATION{ RotationModes::AUTO };
```
- **manuel**, on écrira alors
```cpp
inline constexpr RotationModes PRIORITY_ROTATION{ RotationModes::PIN };
```
En mode **automatique**, la rotation se fait automatiquement toutes les 24 h.  
Em mode **manuel**, il faudra définir en plus la *pin* qui permettra de déclencher une rotation :
```cpp
inline constexpr uint8_t rotationPin{ 10 };
```

## Configuration de la marche forcée
Il est possible de déclencher la marche forcée (certains routeurs appellent cette fonction *Boost*) via une *pin*.  
On peut y relier un micro-interrupteur, une minuterie (ATTENTION, PAS de 230 V sur cette ligne), ou n'importe quel autre contact sec.

Cette fonctionnalité s'active via la ligne :
```cpp
inline constexpr bool OVERRIDE_PIN_PRESENT{ true };
```
Il faudra aussi choisir le *pin* sur laquelle est relié le contact sec :
```cpp
inline constexpr uint8_t forcePin{ 11 };
```

## Arrêt du routage
Il peut être utile de stopper le routage lors d'une absence de plusieurs jours.  
Cela est d'autant plus intéressant si la *pin* de commande est reliée à un contact sec lui-même télécommandable à distance et/ou via une routine Alexa ou similaire.  
De cette façon, il est possible de stopper le routage pendant une absence et le remettre en route par exemple la veille ou l'avant-veille, histoire d'avoir de l'eau chaude (gratuite) au retour.

Cette fonctionnalité s'active via la ligne :
```cpp
inline constexpr bool DIVERSION_PIN_PRESENT{ true };
```
Il faudra aussi choisir la *pin* sur laquelle est relié le contact sec :
```cpp
inline constexpr uint8_t diversionPin{ 12 };
```

*doc non finie*
