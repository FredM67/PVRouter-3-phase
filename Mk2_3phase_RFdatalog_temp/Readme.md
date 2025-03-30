[![en](https://img.shields.io/badge/lang-en-red.svg)](Readme.en.md)

Ce programme est conçu pour être utilisé avec l'IDE Arduino et/ou d'autres IDE de développement comme VSCode + PlatformIO.

- [Utilisation avec Arduino IDE](#utilisation-avec-arduino-ide)
- [Utilisation avec Visual Studio Code](#utilisation-avec-visual-studio-code)
- [Aperçu rapide des fichiers](#aperçu-rapide-des-fichiers)
- [Documentation de développement](#documentation-de-développement)
- [Étalonnage du routeur](#étalonnage-du-routeur)
- [Configuration du programme](#configuration-du-programme)
  - [Type de sortie série](#type-de-sortie-série)
  - [Configuration des sorties TRIAC](#configuration-des-sorties-triac)
  - [Configuration des sorties relais tout-ou-rien](#configuration-des-sorties-relais-tout-ou-rien)
    - [Principe de fonctionnement](#principe-de-fonctionnement)
  - [Configuration du Watchdog](#configuration-du-watchdog)
  - [Configuration du ou des capteurs de température](#configuration-du-ou-des-capteurs-de-température)
    - [Activation de la fonctionnalité](#activation-de-la-fonctionnalité)
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

Vous pouvez également exécuter cette commande dans Powershell : `Get-Childitem –Path C:\ -Include platform.txt -Recurse -ErrorAction SilentlyContinue`. Cela peut prendre quelques secondes/minutes jusqu'à ce que le fichier soit trouvé.

Pour **Linux**, si vous utilisez le package AppImage, vous trouverez ce fichier dans '~/.arduino15/packages/arduino/hardware/avr/1.8.6'. Vous pouvez exécuter `find / -name platform.txt 2>/dev/null` au cas où l'emplacement aurait changé.

Pour **MacOSX**, ce fichier se trouve dans '/Users/[user]/Library/Arduino15/packages/arduino/hardware/avr/1.8.6'.

Ouvrez le fichier dans n'importe quel éditeur de texte (vous aurez besoin des droits d'administrateur) et remplacez le paramètre '**-std=gnu++11**' par '**-std=gnu++17**'. C'est tout !

Si votre IDE Arduino était ouvert, veuillez fermer toutes les instances et le rouvrir.

# Utilisation avec Visual Studio Code

Vous devrez installer des extensions supplémentaires. Les extensions les plus populaires et les plus utilisées pour ce travail sont '*Arduino*' et '*Platform IO*'.  
L'ensemble du projet a été conçu pour être utilisé de façon optimale avec *Platform IO*.

# Aperçu rapide des fichiers

- **Mk2_3phase_RFdatalog_temp.ino** : Ce fichier est nécessaire pour l’IDE Arduino
- **calibration.h** : contient les paramètres d’étalonnage
- **config.h** : les préférences de l’utilisateur sont stockées ici (affectation des broches, fonctionnalités …)
- **config_system.h** : constantes système rarement modifiées
- **constants.h** : quelques constantes — *ne pas modifier*
- **debug.h** : Quelques macros pour la sortie série et le débogage
- **dualtariff.h** : définitions de la fonction double tarif
- **ewma_avg.h** : fonctions de calcul de moyenne EWMA
- **main.cpp** : code source principal
- **movingAvg.h** : code source pour la moyenne glissante
- **processing.cpp** : code source du moteur de traitement
- **processing.h** : prototypes de fonctions du moteur de traitement
- **Readme.md** : ce fichier
- **teleinfo.h**: code source de la fonctionnalité *Télémétrie IoT*
- **types.h** : définitions des types …
- **type_traits.h** : quelques trucs STL qui ne sont pas encore disponibles dans le paquet avr
- **type_traits** : contient des patrons STL manquants
- **utils_dualtariff.h** : code source de la fonctionnalité *gestion Heures Creuses*
- **utils_pins.h** : quelques fonctions d'accès direct aux entrées/sorties du micro-contrôleur
- **utils_relay.h** : code source de la fonctionnalité *diversion par relais*
- **utils_rf.h** : code source de la fonction *RF*
- **utils_temp.h** : code source de la fonctionnalité *Température*
- **utils.h** : fonctions d’aide et trucs divers
- **validation.h** : validation des paramètres, ce code n’est exécuté qu’au moment de la compilation !
- **platformio.ini** : paramètres PlatformIO
- **inject_sketch_name.py** : script d'aide pour PlatformIO
- **Doxyfile** : paramètre pour Doxygen (documentation du code)

L’utilisateur final ne doit éditer QUE les fichiers **calibration.h** et **config.h**.

# Documentation de développement

Vous pouvez commencer à lire la documentation ici [3-phase routeur](https://fredm67.github.io/PVRouter-3-phase/) (en anglais).

# Étalonnage du routeur
Les valeurs d'étalonnage se trouvent dans le fichier **calibration.h**.
Il s'agit de la ligne :
```cpp
inline constexpr float f_powerCal[NO_OF_PHASES]{ 0.05000F, 0.05000F, 0.05000F };
```

Ces valeurs par défaut doivent être déterminées pour assurer un fonctionnement optimal du routeur.

# Configuration du programme

La configuration d'une fonctionnalité suit généralement deux étapes :
- Activation de la fonctionnalité
- Configuration des paramètres de la fonctionnalité

La cohérence de la configuration est vérifiée lors de la compilation. Par exemple, si une *pin* est allouée deux fois par erreur, le compilateur générera une erreur.

## Type de sortie série

Le type de sortie série peut être configuré pour s'adapter à différents besoins. Trois options sont disponibles :

- **HumanReadable** : Sortie lisible par un humain, idéale pour le débogage ou la mise en service.
- **IoT** : Sortie formatée pour des plateformes IoT comme HomeAssistant.
- **EmonCMS** : Sortie compatible avec le format attendu par EmonCMS.

Pour configurer le type de sortie série, modifiez la constante suivante dans le fichier **config.h** :
```cpp
inline constexpr SerialOutputType SERIAL_OUTPUT_TYPE = SerialOutputType::HumanReadable;
```
Remplacez `HumanReadable` par `IoT` ou `EmonCMS` selon vos besoins.

## Configuration des sorties TRIAC

La première étape consiste à définir le nombre de sorties TRIAC :

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

Il faudra activer la fonctionnalité comme ceci :
```cpp
inline constexpr bool RELAY_DIVERSION{ true };
```

Chaque relais nécessite la définition de cinq paramètres :
- le numéro de **pin** sur laquelle est branché le relais
- le **seuil de surplus** avant mise en route (par défaut **1000 W**)
- le **seuil d'import** avant arrêt (par défaut **200 W**)
- la **durée de fonctionnement minimale** en minutes (par défaut **5 min**)
- la **durée d'arrêt minimale** en minutes (par défaut **5 min**).

Exemple de configuration d'un relais :
```cpp
inline constexpr RelayEngine relays{ { { 4, 1000, 200, 10, 10 } } };
```
Dans cet exemple, le relais est connecté sur la *pin* **4**, il se déclenchera à partir de **1000 W** de surplus, s'arrêtera à partir de **200 W** d'import, et a une durée minimale de fonctionnement et d'arrêt de **10 min**.

Pour configurer plusieurs relais, listez simplement les configurations de chaque relais :
```cpp
inline constexpr RelayEngine relays{ { { 4, 1000, 200, 10, 10 },
                                       { 3, 1500, 250, 5, 15 } } };
```
Les relais sont activés dans l'ordre de la liste, et désactivés dans l'ordre inverse.  
Dans tous les cas, les durées minimales de fonctionnement et d'arrêt sont toujours respectées.

### Principe de fonctionnement
Les seuils de surplus et d'import sont calculés en utilisant une moyenne mobile pondérée exponentiellement (EWMA), dans notre cas précis, il s'agit d'une modification d'une moyenne mobile triple exponentiellement pondérée (TEMA).  
Par défaut, cette moyenne est calculée sur une fenêtre d'environ **10 min**. Vous pouvez ajuster cette durée pour l'adapter à vos besoins.  
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

Les relais configurés dans le système sont gérés par un système similaire à une machine à états.
Chaque seconde, le système augmente la durée de l'état actuel de chaque relais et procède avec tous les relais en fonction de la puissance moyenne actuelle :
- si la puissance moyenne actuelle est supérieure au seuil d'import, elle essaie d'éteindre certains relais.
- si la puissance moyenne actuelle est supérieure au seuil de surplus, elle essaie d'allumer plus de relais.

Les relais sont traités dans l'ordre croissant pour le surplus et dans l'ordre décroissant pour l'importation.

Pour chaque relais, la transition ou le changement d'état est géré de la manière suivante :
- si le relais est *OFF* et que la puissance moyenne actuelle est inférieure au seuil de surplus, le relais essaie de passer à l'état *ON*. Cette transition est soumise à la condition que le relais ait été *OFF* pendant au moins la durée *minOFF*.
- si le relais est *ON* et que la puissance moyenne actuelle est supérieure au seuil d'importation, le relais essaie de passer à l'état *OFF*. Cette transition est soumise à la condition que le relais ait été *ON* pendant au moins la durée *minON*.

## Configuration du Watchdog
Un chien de garde, en anglais *watchdog*, est un circuit électronique ou un logiciel utilisé en électronique numérique pour s'assurer qu'un automate ou un ordinateur ne reste pas bloqué à une étape particulière du traitement qu'il effectue.

Ceci est réalisé à l'aide d'une LED qui clignote à la fréquence de 1 Hz, soit toutes les secondes.  
Ainsi, l'utilisateur sait d'une part si son routeur est allumé, et si jamais cette LED ne clignote plus, c'est que l'Arduino s'est bloqué (cas encore jamais rencontré !).  
Un simple appui sur le bouton *Reset* permettra de redémarrage le système sans rien débrancher.

Il faudra activer la fonctionnalité comme ceci :
```cpp
inline constexpr bool WATCHDOG_PIN_PRESENT{ true };
```
et définir la *pin* utilisée, dans l'exemple la *9* :
```cpp
inline constexpr uint8_t watchDogPin{ 9 };
```

## Configuration du ou des capteurs de température
Il est possible de brancher un ou plusieurs capteurs de température Dallas DS18B20.  
Ces capteurs peuvent servir à des fins informatives ou pour contrôler le mode de fonctionnement forcé.

Pour activer cette fonctionnalité, il faudra procéder différemment selon que l'on utilise l'Arduino IDE ou Visual Studio Code avec l'extension PlatformIO.

### Activation de la fonctionnalité

Pour activer cette fonctionnalité, la procédure diffère selon que vous utilisez l'Arduino IDE ou Visual Studio Code avec l'extension PlatformIO.

#### Avec l'Arduino IDE
Activez la ligne suivante en supprimant le commentaire :
```cpp
#define TEMP_ENABLED
```

Si la bibliothèque *OneWire* n'est pas installée, installez-la via le menu **Outils** => **Gérer les bibliothèques…**.  
Recherchez "Onewire" et installez "**OneWire** par Jim Studt, …" en version **2.3.7** ou plus récente.

#### Avec Visual Studio Code et PlatformIO
Sélectionnez la configuration "**env:temperature (Mk2_3phase_RFdatalog_temp)**".

### Configuration du ou des capteurs (commun aux 2 cas précédents)
Pour configurer les capteurs, vous devez entrer leurs adresses.  
Utilisez un programme pour scanner les capteurs connectés.  
Vous pouvez trouver de tels programmes sur Internet ou parmi les exemples fournis avec l'Arduino IDE.  
Il est recommandé de coller une étiquette avec l'adresse de chaque capteur sur son câble.

Entrez les adresses comme suit :
```cpp
inline constexpr TemperatureSensing temperatureSensing{ 4,
                                                        { { 0x28, 0xBE, 0x41, 0x6B, 0x09, 0x00, 0x00, 0xA4 },
                                                          { 0x28, 0x1B, 0xD7, 0x6A, 0x09, 0x00, 0x00, 0xB7 } } };
```
Le nombre *4* en premier paramètre est la *pin* que l'utilisateur aura choisi pour le bus *OneWire*.

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
Décâblez la commande du contacteur Jour/Nuit, qui n'est plus nécessaire.  
Reliez directement une *pin* choisie au contact sec du compteur (bornes *C1* et *C2*).
___
**__ATTENTION__**
Il faut relier **directement**, une paire *pin/masse* avec les bornes *C1/C2* du compteur.  
Il NE doit PAS y avoir de 230 V sur ce circuit !
___

### Configuration logicielle
Activez la fonctionnalité comme suit :
```cpp
inline constexpr bool DUAL_TARIFF{ true };
```
Configurez la *pin* sur laquelle est relié le compteur :
```cpp
inline constexpr uint8_t dualTariffPin{ 3 };
```

Configurez la durée en *heures* de la période d'Heures Creuses (pour l'instant, une seule période est supportée par jour) :
```cpp
inline constexpr uint8_t ul_OFF_PEAK_DURATION{ 8 };
```

Enfin, on définira les modalités de fonctionnement pendant la période d'Heures Creuses :
```cpp
inline constexpr pairForceLoad rg_ForceLoad[NO_OF_DUMPLOADS]{ { -3, 2 } };
```
Il est possible de définir une configuration pour chaque charge indépendamment l'une des autres.
Le premier paramètre de *rg_ForceLoad* détermine la temporisation de démarrage par rapport au début ou à la fin des Heures Creuses :
- si le nombre est positif et inférieur à 24, il s'agit du nombre d'heures,
- si le nombre est négatif supérieur à −24, il s'agit du nombre d'heures par rapport à la fin des Heures Creuses
- si le nombre est positif et supérieur à 24, il s'agit du nombre de minutes,
- si le nombre est négatif inférieur à −24, il s'agit du nombre de minutes par rapport à la fin des Heures Creuses

Le deuxième paramètre détermine la durée de la marche forcée :
- si le nombre est inférieur à 24, il s'agit du nombre d'heures,
- si le nombre est supérieur à 24, il s'agit du nombre de minutes.

Exemples pour mieux comprendre (avec début d'HC à 23:00, jusqu'à 7:00 soit 8 h de durée) :
- ```{ -3, 2 }``` : démarrage **3 heures AVANT** la fin de période (à 4 h du matin), pour une durée de 2 h.
- ```{ 3, 2 }``` : démarrage **3 heures APRÈS** le début de période (à 2 h du matin), pour une durée de 2 h.
- ```{ -150, 2 }``` : démarrage **150 minutes AVANT** la fin de période (à 4:30), pour une durée de 2 h.
- ```{ 3, 180 }``` : démarrage **3 heures APRÈS** le début de période (à 2 h du matin), pour une durée de 180 min.

Pour une durée *infinie* (donc jusqu'à la fin de la période d'HC), utilisez ```UINT16_MAX``` comme deuxième paramètre :
- ```{ -3, UINT16_MAX }``` : démarrage **3 heures AVANT** la fin de période (à 4 h du matin) avec marche forcée jusqu'à la fin de période d'HC.

Si votre système est constitué 2 sorties (```NO_OF_DUMPLOADS``` aura alors une valeur de 2), et que vous souhaitez une marche forcée uniquement sur la 2ᵉ sortie, écrivez :
```cpp
inline constexpr pairForceLoad rg_ForceLoad[NO_OF_DUMPLOADS]{ { 0, 0 },
                                                              { -3, 2 } };
```

## Rotation des priorités
La rotation des priorités est utile lors de l'alimentation d'un chauffe-eau triphasé.  
Elle permet d'équilibrer la durée de fonctionnement des différentes résistances sur une période prolongée.

Mais elle peut aussi être intéressante si on veut permuter les priorités de deux appareils chaque jour (deux chauffe-eau, …).

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
Em mode **manuel**, vous devez également définir la *pin* qui déclenchera la rotation :
```cpp
inline constexpr uint8_t rotationPin{ 10 };
```

## Configuration de la marche forcée
Il est possible de déclencher la marche forcée (certains routeurs appellent cette fonction *Boost*) via une *pin*.  
On peut y relier un micro-interrupteur, une minuterie (ATTENTION, PAS de 230 V sur cette ligne), ou tout autre contact sec.

Pour activer cette fonctionnalité, utilisez le code suivant :
```cpp
inline constexpr bool OVERRIDE_PIN_PRESENT{ true };
```
Vous devez également spécifier la *pin* à laquelle le contact sec est connecté :
```cpp
inline constexpr uint8_t forcePin{ 11 };
```

## Arrêt du routage
Il peut être pratique de désactiver le routage lors d'une absence prolongée.  
Cette fonctionnalité est particulièrement utile si la *pin* de commande est connectée à un contact sec qui peut être contrôlé à distance, par exemple via une routine Alexa ou similaire.  
Ainsi, vous pouvez désactiver le routage pendant votre absence et le réactiver un ou deux jours avant votre retour, afin de disposer d'eau chaude (gratuite) à votre arrivée.

Pour activer cette fonctionnalité, utilisez le code suivant :
```cpp
inline constexpr bool DIVERSION_PIN_PRESENT{ true };
```
Vous devez également spécifier la *pin* à laquelle le contact sec est connecté :
```cpp
inline constexpr uint8_t diversionPin{ 12 };
```

*doc non finie*
