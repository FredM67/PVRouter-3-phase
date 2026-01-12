[![en](https://img.shields.io/badge/lang-en-red.svg)](Readme.en.md)

Ce programme est conçu pour être utilisé avec l'IDE Arduino et/ou d'autres IDE de développement comme VSCode + PlatformIO.

# Table des matières
- [Table des matières](#table-des-matières)
- [Utilisation avec Visual Studio Code (recommandé)](#utilisation-avec-visual-studio-code-recommandé)
- [Utilisation avec Arduino IDE](#utilisation-avec-arduino-ide)
- [Aperçu rapide des fichiers](#aperçu-rapide-des-fichiers)
  - [Documentation technique](#documentation-technique)
- [Documentation de développement](#documentation-de-développement)
- [Étalonnage du routeur](#étalonnage-du-routeur)
- [Documentation d'analyse et outils](#documentation-danalyse-et-outils)
- [Configuration du programme](#configuration-du-programme)
  - [Type de sortie série](#type-de-sortie-série)
  - [Configuration des sorties TRIAC](#configuration-des-sorties-triac)
  - [Configuration des sorties relais tout-ou-rien](#configuration-des-sorties-relais-tout-ou-rien)
    - [Principe de fonctionnement](#principe-de-fonctionnement)
  - [Configuration du Watchdog](#configuration-du-watchdog)
  - [Configuration du ou des capteurs de température](#configuration-du-ou-des-capteurs-de-température)
    - [Activation de la fonctionnalité](#activation-de-la-fonctionnalité)
    - [Configuration du ou des capteurs (commun aux 2 cas précédents)](#configuration-du-ou-des-capteurs-commun-aux-2-cas-précédents)
  - [Gestion des Heures Creuses et boost programmé (dual tariff)](#gestion-des-heures-creuses-et-boost-programmé-dual-tariff)
    - [Configuration matérielle](#configuration-matérielle)
    - [Configuration logicielle](#configuration-logicielle)
    - [Configuration du boost programmé (rg\_ForceLoad)](#configuration-du-boost-programmé-rg_forceload)
    - [Exemples visuels](#exemples-visuels)
    - [Configuration pour plusieurs charges](#configuration-pour-plusieurs-charges)
    - [Aide-mémoire](#aide-mémoire)
  - [Rotation des priorités](#rotation-des-priorités)
  - [Boost manuel (déclenché par pin)](#boost-manuel-déclenché-par-pin)
    - [Fonctionnement](#fonctionnement)
    - [Quand utiliser le boost manuel](#quand-utiliser-le-boost-manuel)
    - [Configuration](#configuration)
    - [Cibler les charges et relais](#cibler-les-charges-et-relais)
    - [Exemples de configuration](#exemples-de-configuration)
    - [Câblage](#câblage)
    - [Exemples pratiques](#exemples-pratiques)
  - [Arrêt du routage](#arrêt-du-routage)
- [Configuration avancée du programme](#configuration-avancée-du-programme)
  - [Paramètre `DIVERSION_START_THRESHOLD_WATTS`](#paramètre-diversion_start_threshold_watts)
  - [Paramètre `REQUIRED_EXPORT_IN_WATTS`](#paramètre-required_export_in_watts)
- [Configuration avec la carte d'extension ESP32](#configuration-avec-la-carte-dextension-esp32)
  - [Correspondance des broches](#correspondance-des-broches)
  - [Configuration du pont `TEMP`](#configuration-du-pont-temp)
  - [Configuration recommandée](#configuration-recommandée)
    - [Configuration de base recommandée](#configuration-de-base-recommandée)
    - [Fonctionnalités additionnelles recommandées](#fonctionnalités-additionnelles-recommandées)
    - [Installation des sondes de température](#installation-des-sondes-de-température)
  - [Liaison avec Home Assistant](#liaison-avec-homeassistant)
- [Configuration sans carte d'extension](#configuration-sans-carte-dextension)
- [Dépannage](#dépannage)
- [Contribuer](#contribuer)

# Utilisation avec Visual Studio Code (recommandé)

Vous devrez installer des extensions supplémentaires. Les extensions les plus populaires et les plus utilisées pour ce travail sont '*Platform IO*' et '*Arduino*'.
L'ensemble du projet a été conçu pour être utilisé de façon optimale avec *Platform IO*.

# Utilisation avec Arduino IDE

Pour utiliser ce programme avec l'IDE Arduino, vous devez télécharger et installer la dernière version de l'IDE Arduino. Choisissez la version "standard", PAS la version du Microsoft Store. Optez pour la version "Win 10 et plus récent, 64 bits" ou la version "MSI installer".

Comme le code est optimisé avec l'une des dernières normes C++, vous devez modifier un fichier de configuration pour activer C++17. Vous trouverez le fichier '**platform.txt**' dans le chemin d'installation de l'IDE Arduino.

Pour **Windows**, vous trouverez généralement le fichier dans '**C:\Program Files (x86)\Arduino\hardware\arduino\avr**' et/ou dans '**%LOCALAPPDATA%\Arduino15\packages\arduino\hardware\avr\x.y.z**' où **'x.y.z**' est la version du package Arduino AVR Boards.

Vous pouvez également exécuter cette commande dans Powershell : `Get-Childitem –Path C:\ -Include platform.txt -Recurse -ErrorAction SilentlyContinue`. Cela peut prendre quelques secondes/minutes jusqu'à ce que le fichier soit trouvé.

Pour **Linux**, si vous utilisez le package AppImage, vous trouverez ce fichier dans '~/.arduino15/packages/arduino/hardware/avr/1.8.6'. Vous pouvez exécuter `find / -name platform.txt 2>/dev/null` au cas où l'emplacement aurait changé.

Pour **MacOSX**, ce fichier se trouve dans '/Users/[user]/Library/Arduino15/packages/arduino/hardware/avr/1.8.6'.

Ouvrez le fichier dans n'importe quel éditeur de texte (vous aurez besoin des droits d'administrateur) et remplacez le paramètre '**-std=gnu++11**' par '**-std=gnu++17**'. C'est tout !

Si votre IDE Arduino était ouvert, veuillez fermer toutes les instances et le rouvrir.
___
> [!WARNING]
> En cas d'utilisation de la libraire **ArduinoJson**, il faudra impérativement installer une version **6.x**.
> La version 7.x, certes plus actuelle, est devenue trop lourde pour un Atmega328P.
___

# Aperçu rapide des fichiers

- **Mk2_3phase_RFdatalog_temp.ino** : Ce fichier est nécessaire pour l'IDE Arduino
- **calibration.h** : contient les paramètres d'étalonnage
- **config.h** : les préférences de l'utilisateur sont stockées ici (affectation des broches, fonctionnalités …)
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
- **utils.h** : fonctions d'aide et trucs divers
- **validation.h** : validation des paramètres, ce code n'est exécuté qu'au moment de la compilation !
- **platformio.ini** : paramètres PlatformIO
- **inject_sketch_name.py** : script d'aide pour PlatformIO
- **Doxyfile** : paramètre pour Doxygen (documentation du code)

L'utilisateur final ne doit éditer QUE les fichiers **calibration.h** et **config.h**.

## Documentation technique
Le dossier **[docs/](docs/)** contient la documentation technique détaillée :
- **[Architecture logicielle](docs/architecture.md)** - Conception et organisation des modules
- **[Performances](docs/performance.md)** - Analyses de timing et optimisations

# Documentation de développement

Vous pouvez commencer à lire la documentation ici [3-phase routeur](https://fredm67.github.io/Mk2PVRouter-3-phase/) (en anglais).

# Étalonnage du routeur
Les valeurs d'étalonnage se trouvent dans le fichier **calibration.h**.
Il s'agit de la ligne :
```cpp
inline constexpr float f_powerCal[NO_OF_PHASES]{ 0.05000F, 0.05000F, 0.05000F };
```

Ces valeurs par défaut doivent être déterminées pour assurer un fonctionnement optimal du routeur.

# Documentation d'analyse et outils

📊 **[Outils d'Analyse et Documentation Technique](../analysis/README.md)** [![en](https://img.shields.io/badge/lang-en-red.svg)](../analysis/README.en.md)

Cette section contient des outils d'analyse avancés et de la documentation technique pour :

- **🔄 Filtrage EWMA/TEMA** : Analyse de l'immunité aux nuages et optimisation des filtres
- **📈 Analyse de performance** : Scripts de visualisation et benchmarks
- **⚙️ Guide de réglage** : Documentation pour l'optimisation des paramètres
- **📊 Graphiques techniques** : Comparaisons visuelles des algorithmes de filtrage

> **Utilisateurs avancés :** Ces outils vous aideront à comprendre et optimiser le comportement du routeur PV, notamment pour les installations avec variabilité de production solaire ou systèmes de batteries.

# Configuration du programme

La configuration d'une fonctionnalité suit généralement deux étapes :
- Activation de la fonctionnalité
- Configuration des paramètres de la fonctionnalité

La cohérence de la configuration est vérifiée lors de la compilation. Par exemple, si une *pin* est allouée deux fois par erreur, le compilateur générera une erreur.

## Type de sortie série

Le type de sortie série peut être configuré pour s'adapter à différents besoins. Trois options sont disponibles :

- **HumanReadable** : Sortie lisible par un humain, idéale pour le débogage ou la mise en service.
- **IoT** : Sortie formatée pour des plateformes IoT comme Home Assistant.
- **JSON** : Sortie formatée pour des plateformes comme EmonCMS (JSON).

Pour configurer le type de sortie série, modifiez la constante suivante dans le fichier **config.h** :
```cpp
inline constexpr SerialOutputType SERIAL_OUTPUT_TYPE = SerialOutputType::HumanReadable;
```
Remplacez `HumanReadable` par `IoT` ou `JSON` selon vos besoins.

## Configuration des sorties TRIAC

La première étape consiste à définir le nombre de sorties TRIAC :

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

Il faudra activer la fonctionnalité comme ceci :
```cpp
inline constexpr bool RELAY_DIVERSION{ true };
```

Chaque relais nécessite la définition de cinq paramètres :
- le numéro de **pin** sur laquelle est branché le relais
- le **seuil de surplus** avant mise en route (par défaut **1000 W**)
- le **seuil d'import** avant arrêt (par défaut **200 W**)
- la **durée de fonctionnement minimale** en minutes (par défaut **5 min**)
- la **durée d'arrêt minimale** en minutes (par défaut **5 min**).

Exemple de configuration d'un relais :
```cpp
inline constexpr RelayEngine relays{ { { 4, 1000, 200, 10, 10 } } };
```
Dans cet exemple, le relais est connecté sur la *pin* **4**, il se déclenchera à partir de **1000 W** de surplus, s'arrêtera à partir de **200 W** d'import, et a une durée minimale de fonctionnement et d'arrêt de **10 min**.

Pour configurer plusieurs relais, listez simplement les configurations de chaque relais :
```cpp
inline constexpr RelayEngine relays{ { { 4, 1000, 200, 10, 10 },
                                       { 3, 1500, 250, 5, 15 } } };
```
Les relais sont activés dans l'ordre de la liste, et désactivés dans l'ordre inverse.
Dans tous les cas, les durées minimales de fonctionnement et d'arrêt sont toujours respectées.

### Principe de fonctionnement
Les seuils de surplus et d'import sont calculés en utilisant une moyenne mobile pondérée exponentiellement (EWMA), dans notre cas précis, il s'agit d'une modification d'une moyenne mobile triple exponentiellement pondérée (TEMA).
Par défaut, cette moyenne est calculée sur une fenêtre d'environ **10 min**. Vous pouvez ajuster cette durée pour l'adapter à vos besoins.
Il est possible de la rallonger mais aussi de la raccourcir.
Pour des raisons de performances de l'Arduino, la durée choisie sera arrondie à une durée proche qui permettra de faire les calculs sans impacter les performances du routeur.

La durée de la fenêtre temporelle est contrôlée par le paramètre `RELAY_FILTER_DELAY` dans le fichier de configuration.

Si l'utilisateur souhaite plutôt une fenêtre de 15 min, il suffira d'écrire :
```cpp
inline constexpr RelayEngine relays{ MINUTES(15), { { 3, 1000, 200, 1, 1 } } };
```
___
> [!NOTE]
> La macro `MINUTES()` convertit automatiquement la valeur en paramètre template. Aucun suffixe spécial n'est nécessaire !
___

Les relais configurés dans le système sont gérés par un système similaire à une machine à états.
Chaque seconde, le système augmente la durée de l'état actuel de chaque relais et procède avec tous les relais en fonction de la puissance moyenne actuelle :
- si la puissance moyenne actuelle est supérieure au seuil d'import, elle essaie d'éteindre certains relais.
- si la puissance moyenne actuelle est supérieure au seuil de surplus, elle essaie d'allumer plus de relais.

Les relais sont traités dans l'ordre croissant pour le surplus et dans l'ordre décroissant pour l'importation.

Pour chaque relais, la transition ou le changement d'état est géré de la manière suivante :
- si le relais est *OFF* et que la puissance moyenne actuelle est inférieure au seuil de surplus, le relais essaie de passer à l'état *ON*. Cette transition est soumise à la condition que le relais ait été *OFF* pendant au moins la durée *minOFF*.
- si le relais est *ON* et que la puissance moyenne actuelle est supérieure au seuil d'importation, le relais essaie de passer à l'état *OFF*. Cette transition est soumise à la condition que le relais ait été *ON* pendant au moins la durée *minON*.

> [!NOTE]
> **Installations avec batteries :** Pour une configuration optimale des relais avec systèmes de batteries, consultez le **[Guide de Configuration pour Systèmes Batterie](docs/BATTERY_CONFIGURATION_GUIDE.md)** [![en](https://img.shields.io/badge/lang-en-red.svg)](docs/BATTERY_CONFIGURATION_GUIDE.en.md)

## Configuration du Watchdog
Un chien de garde, en anglais *watchdog*, est un circuit électronique ou un logiciel utilisé en électronique numérique pour s'assurer qu'un automate ou un ordinateur ne reste pas bloqué à une étape particulière du traitement qu'il effectue.

Ceci est réalisé à l'aide d'une LED qui clignote à la fréquence de 1 Hz, soit toutes les secondes.
Ainsi, l'utilisateur sait d'une part si son routeur est allumé, et si jamais cette LED ne clignote plus, c'est que l'Arduino s'est bloqué (cas encore jamais rencontré !).
Un simple appui sur le bouton *Reset* permettra de redémarrage le système sans rien débrancher.

Il faudra activer la fonctionnalité comme ceci :
```cpp
inline constexpr bool WATCHDOG_PIN_PRESENT{ true };
```
et définir la *pin* utilisée, dans l'exemple la *9* :
```cpp
inline constexpr uint8_t watchDogPin{ 9 };
```

## Configuration du ou des capteurs de température
Il est possible de brancher un ou plusieurs capteurs de température Dallas DS18B20.
Ces capteurs peuvent servir à des fins informatives ou pour contrôler le mode boost.

Pour activer cette fonctionnalité, il faudra procéder différemment selon que l'on utilise l'Arduino IDE ou Visual Studio Code avec l'extension PlatformIO.

Par défaut, la sortie `D3` est utilisée pour la sortie du capteur de température et dispose déjà d'un pull-up.
Si vous souhaitez utiliser une autre pin, il faudra rajouter un *pull-up* sur la pin utilisée.

### Activation de la fonctionnalité

Pour activer cette fonctionnalité, la procédure diffère selon que vous utilisez l'Arduino IDE ou Visual Studio Code avec l'extension PlatformIO.

#### Avec l'Arduino IDE
Activez la ligne suivante en supprimant le commentaire :
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

Entrez les adresses comme suit :
```cpp
inline constexpr TemperatureSensing temperatureSensing{ 4,
                                                        { { 0x28, 0xBE, 0x41, 0x6B, 0x09, 0x00, 0x00, 0xA4 },
                                                          { 0x28, 0x1B, 0xD7, 0x6A, 0x09, 0x00, 0x00, 0xB7 } } };
```
Le nombre *4* en premier paramètre est la *pin* que l'utilisateur aura choisi pour le bus *OneWire*.

___
> [!NOTE]
> Plusieurs capteurs peuvent être branchés sur le même câble.
> Sur Internet vous trouverez tous les détails concernant la topologie utilisable avec ce genre de capteurs.
___

## Gestion des Heures Creuses et boost programmé (dual tariff)

Cette fonctionnalité permet au routeur de gérer automatiquement le chauffage pendant les périodes d'Heures Creuses. Elle est utile pour :
- Chauffer l'eau la nuit quand l'électricité est moins chère
- Garantir de l'eau chaude le matin si le surplus solaire a été insuffisant pendant la journée
- Limiter la durée de chauffe pour éviter la surchauffe (optionnellement avec un capteur de température)

### Configuration matérielle

Décâblez la commande du contacteur Jour/Nuit, qui n'est plus nécessaire.
Reliez directement une *pin* choisie au contact sec du compteur (bornes *C1* et *C2*).

> [!WARNING]
> Il faut relier **directement**, une paire *pin/masse* avec les bornes *C1/C2* du compteur.
> Il NE doit PAS y avoir de 230 V sur ce circuit !

### Configuration logicielle

**Étape 1 :** Activez la fonctionnalité :
```cpp
inline constexpr bool DUAL_TARIFF{ true };
```

**Étape 2 :** Configurez la *pin* sur laquelle est relié le compteur :
```cpp
inline constexpr uint8_t dualTariffPin{ 3 };
```

**Étape 3 :** Configurez la durée en heures de la période d'Heures Creuses (pour l'instant, une seule période est supportée par jour) :
```cpp
inline constexpr uint8_t ul_OFF_PEAK_DURATION{ 8 };
```

**Étape 4 :** Configurez le timing du boost programmé pour chaque charge.

### Configuration du boost programmé (rg_ForceLoad)

Le tableau `rg_ForceLoad` définit **quand** et **combien de temps** chaque charge doit être en boost pendant la période d'Heures Creuses.

```cpp
inline constexpr pairForceLoad rg_ForceLoad[NO_OF_DUMPLOADS]{ { HEURE_DEBUT, DUREE } };
```

Chaque charge a deux paramètres : `{ HEURE_DEBUT, DUREE }`

#### Comprendre la ligne du temps

```
Exemple de période HC : 23:00 à 07:00 (8 heures)

        23:00                                           07:00
          |================== HEURES CREUSES =============|
          |                                              |
     DEBUT ──────────────────────────────────────────► FIN
          │                                              │
          │  Les valeurs positives                       │
          │  comptent à partir d'ici ───►                │
          │                                              │
          │                      ◄─── Les valeurs        │
          │                           négatives comptent │
          │                           à partir d'ici     │
```

#### Paramètre 1 : HEURE_DEBUT (quand démarrer)

| Valeur | Signification | Exemple (HC 23:00-07:00) |
|--------|---------------|--------------------------|
| `0` | **Désactivé** - pas de boost pour cette charge | - |
| `1` à `23` | Heures **après** le DÉBUT des HC | `3` = démarrage à 02:00 (23:00 + 3h) |
| `-1` à `-23` | Heures **avant** la FIN des HC | `-3` = démarrage à 04:00 (07:00 - 3h) |
| `24` ou plus | Minutes **après** le DÉBUT des HC | `90` = démarrage à 00:30 (23:00 + 90min) |
| `-24` ou moins | Minutes **avant** la FIN des HC | `-90` = démarrage à 05:30 (07:00 - 90min) |

> [!NOTE]
> **Pourquoi 24 ?** La valeur 24 sert de seuil pour distinguer les heures des minutes.
> Les valeurs de 1 à 23 sont interprétées comme des heures, les valeurs 24+ sont interprétées comme des minutes.

#### Paramètre 2 : DUREE (combien de temps)

| Valeur | Signification |
|--------|---------------|
| `0` | **Désactivé** - pas de boost |
| `1` à `23` | Durée en **heures** |
| `24` ou plus | Durée en **minutes** |
| `UINT16_MAX` | Jusqu'à la **fin** de la période HC |

> [!IMPORTANT]
> **Le boost s'arrête toujours à la fin de la période d'Heures Creuses**, quelle que soit la durée configurée.
> Si vous définissez une durée qui dépasse la fin des HC, le boost sera interrompu.

### Exemples visuels

Tous les exemples supposent une période HC de **23:00 à 07:00** (8 heures) :

**Exemple 1 :** `{ -3, 2 }` - Démarrage 3 heures avant la fin, durée 2 heures
```
23:00                              04:00    06:00    07:00
  |====================================|======|========|
                                       |BOOST |
                                       └──2h──┘
```
Résultat : Le boost fonctionne de **04:00 à 06:00**

**Exemple 2 :** `{ 2, 3 }` - Démarrage 2 heures après le début, durée 3 heures
```
23:00    01:00          04:00                        07:00
  |========|=============|==============================|
           |────BOOST────|
           └─────3h──────┘
```
Résultat : Le boost fonctionne de **01:00 à 04:00**

**Exemple 3 :** `{ -90, 120 }` - Démarrage 90 minutes avant la fin, durée de 120 minutes (mais limitée)
```
23:00                              05:30    07:00
  |====================================|======|
                                       |BOOST | ← s'arrête ici (fin des HC)
                                       └─90min─┘
```
Résultat : Le boost fonctionne de **05:30 à 07:00** (s'arrête à la fin des HC, pas à 07:30)

> [!NOTE]
> **Le boost s'arrête toujours à la fin de la période d'Heures Creuses**, même si la durée configurée est plus longue.
> Dans cet exemple, seules 90 minutes de boost ont lieu au lieu des 120 minutes configurées.

**Exemple 4 :** `{ 1, UINT16_MAX }` - Démarrage 1 heure après le début, jusqu'à la fin
```
23:00    00:00                                       07:00
  |========|=========================================|
           |──────────────BOOST──────────────────────|
```
Résultat : Le boost fonctionne de **00:00 à 07:00**

### Configuration pour plusieurs charges

Chaque charge peut avoir sa propre programmation de boost. Utilisez `{ 0, 0 }` pour désactiver le boost d'une charge spécifique.

**Exemple :** 2 charges, boost uniquement sur la deuxième :
```cpp
inline constexpr pairForceLoad rg_ForceLoad[NO_OF_DUMPLOADS]{
    { 0, 0 },      // Charge #1 : pas de boost programmé
    { -3, 2 }      // Charge #2 : boost 3h avant la fin, pendant 2h
};
```

**Exemple :** 3 charges avec des programmations différentes :
```cpp
inline constexpr pairForceLoad rg_ForceLoad[NO_OF_DUMPLOADS]{
    { -4, 2 },          // Charge #1 : 03:00-05:00 (dernier recours, si l'eau est encore froide)
    { -2, UINT16_MAX }, // Charge #2 : 05:00-07:00 (complément avant le matin)
    { 0, 0 }            // Charge #3 : pas de boost programmé
};
```

### Aide-mémoire

| Vous voulez... | Utilisez ceci |
|----------------|---------------|
| Désactiver le boost | `{ 0, 0 }` |
| Démarrer 2h après le début des HC, pendant 3h | `{ 2, 3 }` |
| Démarrer 3h avant la fin des HC, pendant 2h | `{ -3, 2 }` |
| Démarrer 90min après le début des HC, pendant 2h | `{ 90, 2 }` |
| Démarrer 90min avant la fin des HC, jusqu'à la fin | `{ -90, UINT16_MAX }` |

## Rotation des priorités
La rotation des priorités est utile lors de l'alimentation d'un chauffe-eau triphasé.
Elle permet d'équilibrer la durée de fonctionnement des différentes résistances sur une période prolongée.

Mais elle peut aussi être intéressante si on veut permuter les priorités de deux appareils chaque jour (deux chauffe-eau, …).

Une fois n'est pas coutume, l'activation de cette fonction possède 2 modes :
- **automatique**, on spécifiera alors
```cpp
inline constexpr RotationModes PRIORITY_ROTATION{ RotationModes::AUTO };
```
- **manuel**, on écrira alors
```cpp
inline constexpr RotationModes PRIORITY_ROTATION{ RotationModes::PIN };
```
En mode **automatique**, la rotation se fait automatiquement toutes les 24 h.
Em mode **manuel**, vous devez également définir la *pin* qui déclenchera la rotation :
```cpp
inline constexpr uint8_t rotationPin{ 10 };
```

## Boost manuel (déclenché par pin)

Contrairement au [boost programmé](#configuration-du-boost-programmé-rg_forceload) qui s'exécute automatiquement pendant les Heures Creuses, le **boost manuel** vous permet de déclencher le chauffage à la demande via des interrupteurs physiques, des boutons, des minuteries ou des systèmes domotiques.

### Fonctionnement

```
┌─────────────────┐
│   Pin physique  │──── Quand le contact se ferme ───► Les charges/relais passent à 100%
│  (contact sec)  │──── Quand le contact s'ouvre  ───► Fonctionnement normal du routeur
└─────────────────┘
```

### Quand utiliser le boost manuel

- **Bouton dans la salle de bain** : Chauffer rapidement l'eau avant une douche
- **Minuterie externe** : Forcer toutes les charges pendant 30 minutes à une heure précise
- **Domotique** : Déclencher le boost via Alexa, Home Assistant ou similaire
- **Chauffage d'urgence** : Passer outre le fonctionnement normal quand plus d'eau chaude est nécessaire

### Configuration

**Étape 1 :** Activez la fonctionnalité :
```cpp
inline constexpr bool OVERRIDE_PIN_PRESENT{ true };
```

**Étape 2 :** Définissez quelle(s) pin(s) contrôle(nt) quelle(s) charge(s) :
```cpp
inline constexpr OverridePins overridePins{ { { PIN, CIBLES } } };
```

### Cibler les charges et relais

Il existe **deux méthodes** pour spécifier quelles charges/relais activer :

#### Méthode 1 : Par index avec les macros (recommandé)

| Macro | Description |
|-------|-------------|
| `LOAD(n)` | Charge par index (0 = première charge, 1 = deuxième, etc.) |
| `RELAY(n)` | Relais par index (0 = premier relais, 1 = deuxième, etc.) |
| `ALL_LOADS()` | Toutes les charges configurées |
| `ALL_RELAYS()` | Tous les relais configurés |
| `ALL_LOADS_AND_RELAYS()` | Tout le système |

#### Méthode 2 : Par numéro de pin physique

Vous pouvez aussi utiliser directement le **numéro de pin physique**. Cela active la charge ou le relais connecté à cette pin.

```cpp
{ 3, { 5 } }    // La pin D3 déclenche : active la charge/relais sur la pin 5
{ 3, { 5, 7 } } // La pin D3 déclenche : active les charges/relais sur les pins 5 et 7
```

#### Combiner les deux méthodes

Vous pouvez mélanger les deux méthodes dans la même configuration :

```cpp
{ 3, { 5, LOAD(1) } }  // Pin D3 : active la pin 5 ET la charge #1
```

### Exemples de configuration

**Simple :** Un bouton contrôle tout
```cpp
inline constexpr OverridePins overridePins{ {
    { 11, ALL_LOADS_AND_RELAYS() }    // Pin D11 : boost de tout le système
} };
```

**Avec les macros :** Cibler par index de charge/relais
```cpp
inline constexpr OverridePins overridePins{ {
    { 3, { LOAD(0) } },               // Pin D3 : boost de la charge #0 (première charge)
    { 4, { RELAY(0), RELAY(1) } }     // Pin D4 : boost des relais #0 et #1
} };
```

**Avec les numéros de pins :** Cibler par pin physique
```cpp
inline constexpr OverridePins overridePins{ {
    { 3, { 5 } },                     // Pin D3 : boost de ce qui est sur la pin 5
    { 4, { 5, 7 } }                   // Pin D4 : boost des pins 5 et 7
} };
```

**Approche mixte :** Combiner les deux méthodes
```cpp
inline constexpr OverridePins overridePins{ {
    { 3, { 5, LOAD(1) } },            // Pin D3 : pin 5 + charge #1
    { 4, ALL_LOADS() },               // Pin D4 : toutes les charges
    { 11, { LOAD(1), LOAD(2) } },     // Pin D11 : charges #1 et #2
    { 12, ALL_LOADS_AND_RELAYS() }    // Pin D12 : tout le système
} };
```

### Câblage

Connectez chaque pin configurée à un **contact sec** (interrupteur sans tension) :

```
Pin Arduino ────┬──── Interrupteur/Bouton ────┬──── GND
                │                              │
          (pull-up interne)              (ferme le circuit)
```

- **Contact fermé** = boost actif (charges à pleine puissance)
- **Contact ouvert** = fonctionnement normal du routeur

> [!NOTE]
> Les pins sont configurées avec des résistances pull-up internes. Aucune résistance externe n'est nécessaire.

### Exemples pratiques

| Installation | Configuration | Résultat |
|--------------|---------------|----------|
| Bouton boost salle de bain | `{ 3, { LOAD(0) } }` | Appui sur le bouton → chauffe-eau à 100% |
| Minuterie 30 min | `{ 4, ALL_LOADS() }` | La minuterie ferme le contact → toutes les charges en boost pendant la durée de la minuterie |
| Intégration domotique | `{ 11, ALL_LOADS_AND_RELAYS() }` | Module ESP32/relais déclenche le boost de tout le système |

## Arrêt du routage
Il peut être pratique de désactiver le routage lors d'une absence prolongée.
Cette fonctionnalité est particulièrement utile si la *pin* de commande est connectée à un contact sec qui peut être contrôlé à distance, par exemple via une routine Alexa ou similaire.
Ainsi, vous pouvez désactiver le routage pendant votre absence et le réactiver un ou deux jours avant votre retour, afin de disposer d'eau chaude (gratuite) à votre arrivée.

Pour activer cette fonctionnalité, utilisez le code suivant :
```cpp
inline constexpr bool DIVERSION_PIN_PRESENT{ true };
```
Vous devez également spécifier la *pin* à laquelle le contact sec est connecté :
```cpp
inline constexpr uint8_t diversionPin{ 12 };
```

# Configuration avancée du programme

Ces paramètres se trouvent dans le fichier `config_system.h`.

## Paramètre `DIVERSION_START_THRESHOLD_WATTS`
Le paramètre `DIVERSION_START_THRESHOLD_WATTS` définit un seuil de surplus avant tout routage vers les charges configurées sur le routeur. Elle est principalement destinée aux installations avec batteries de stockage.
Par défaut, cette valeur est réglée à 0 W.
En réglant ce paramètre à 50 W par exemple, le routeur ne démarrera le routage qu'à partir du moment où 50 W de surplus sera disponible. Une fois le routage démarré, la totalité du surplus sera routé.
Cette fonctionnalité permet d'établir une hiérarchie claire dans l'utilisation de l'énergie produite, en privilégiant le stockage d'énergie sur la consommation immédiate. Vous pouvez ajuster cette valeur selon la réactivité du système de charge des batteries et vos priorités d'utilisation de l'énergie.

> [!IMPORTANT]
> Ce paramètre concerne uniquement la condition de démarrage du routage.
> Une fois le seuil atteint et le routage démarré, la **totalité** du surplus devient disponible pour les charges.

## Paramètre `REQUIRED_EXPORT_IN_WATTS`
Le paramètre `REQUIRED_EXPORT_IN_WATTS` détermine la quantité minimale d'énergie que le système doit réserver pour l'exportation ou l'importation vers le réseau électrique avant de dévier le surplus vers les charges contrôlées.
Par défaut réglé à 0 W, ce paramètre peut être utilisé pour garantir une exportation constante vers le réseau, par exemple pour respecter des accords de revente d'électricité.
Une valeur négative obligera le routeur à consommer cette puissance depuis le réseau. Cela peut être utile voire nécessaire pour les installations configurées en *zéro injection* afin d'amorcer la production solaire.

> [!IMPORTANT]
> Contrairement au premier paramètre, celui-ci représente un décalage permanent qui est continuellement soustrait du surplus disponible.
> Si réglé à 20 W par exemple, le système réservera **toujours** 20 W pour l'exportation, indépendamment des autres conditions.

# Configuration avec la carte d'extension ESP32

La carte d'extension ESP32 permet une intégration simple et fiable entre le Mk2PVRouter et un ESP32 pour le contrôle à distance via Home Assistant. Cette section détaille comment configurer correctement le Mk2PVRouter lorsque vous utilisez cette carte d'extension.

## Correspondance des broches
Lorsque vous utilisez la carte d'extension ESP32, les connexions entre le Mk2PVRouter et l'ESP32 sont prédéfinies comme suit :

| ESP32  | Mk2PVRouter | Fonction                              |
| ------ | ----------- | ------------------------------------- |
| GPIO12 | D12         | Entrée/Sortie numérique - Usage libre |
| GPIO13 | D11         | Entrée/Sortie numérique - Usage libre |
| GPIO14 | D13         | Entrée/Sortie numérique - Usage libre |
| GPIO27 | D10         | Entrée/Sortie numérique - Usage libre |
| GPIO5  | DS18B20     | Bus 1-Wire pour sondes de température |

## Configuration du pont `TEMP`
**Important** : Si vous souhaitez que l'ESP32 contrôle les sondes de température (recommandé pour l'intégration avec Home Assistant), **le pont `TEMP` sur la carte mère du routeur ne doit pas être soudé**.
- **Pont `TEMP` non soudé** : L'ESP32 contrôle les sondes de température via GPIO5.
- **Pont `TEMP` soudé** : Le Mk2PVRouter contrôle les sondes de température via D3.

## Configuration recommandée
Pour une utilisation optimale avec Home Assistant, il est recommandé d'activer au minimum les fonctions suivantes :

### Configuration de base recommandée
```cpp
// Type de sortie série pour l'intégration IoT
inline constexpr SerialOutputType SERIAL_OUTPUT_TYPE = SerialOutputType::IoT;

// Fonctions essentielles recommandées
inline constexpr bool DIVERSION_PIN_PRESENT{ true };    // Arrêt du routage
inline constexpr bool OVERRIDE_PIN_PRESENT{ true };     // Boost

// Pin configuration selon la correspondance de la carte d'extension
inline constexpr uint8_t diversionPin{ 12 };     // D12 - arrêt du routage

// Configuration du boost flexible
inline constexpr OverridePins overridePins{ { { 11, ALL_LOADS_AND_RELAYS() } } }; // D11 - boost

// Configuration pour les sondes de température
// IMPORTANT: Désactiver la gestion de température dans le Mk2PVRouter
// si l'ESP32 gère les sondes (pont TEMP non soudé)
inline constexpr bool TEMP_SENSOR_PRESENT{ false };  // Désactivé car géré par l'ESP32
```

> [!NOTE]
> La configuration de la sortie série sur `SerialOutputType::IoT` n'est pas strictement obligatoire pour le fonctionnement du routeur. Cependant, elle est nécessaire si vous souhaitez exploiter les données du routeur dans Home Assistant (puissance instantanée, statistiques, etc.). Sans cette configuration, seules les fonctions de contrôle (boost, arrêt routage) seront disponibles dans Home Assistant.

### Fonctionnalités additionnelles recommandées
Pour une intégration encore plus complète, vous pouvez également ajouter ces fonctionnalités :
```cpp
// Rotation des priorités via pin (optionnel)
inline constexpr RotationModes PRIORITY_ROTATION{ RotationModes::PIN };
inline constexpr uint8_t rotationPin{ 10 };      // D10 - rotation des priorités
```

### Installation des sondes de température
Pour l'installation des sondes de température :
- Assurez-vous que le pont `TEMP` n'est **pas** soudé sur la carte mère du routeur
- Connectez vos sondes DS18B20 directement via les connecteurs dédiés sur la carte mère du Mk2PVRouter
- Configurez les sondes dans ESPHome (aucune configuration n'est nécessaire côté Mk2PVRouter)

L'utilisation de l'ESP32 pour gérer les sondes de température présente plusieurs avantages :
- Visualisation des températures directement dans Home Assistant
- Possibilité de créer des automatisations basées sur les températures
- Configuration plus flexible des sondes sans avoir à reprogrammer le Mk2PVRouter

## Liaison avec Home Assistant
Une fois votre MkPVRouter configuré avec la carte d'extension ESP32, vous pourrez :
- Contrôler à distance l'activation/désactivation du routage (idéal pendant les absences)
- Déclencher un boost à distance
- Surveiller les températures en temps réel
- Créer des scénarios d'automatisation avancés combinant les données de production solaire et les températures

Pour plus de détails sur la configuration d'ESPHome et l'intégration avec Home Assistant, consultez la [documentation détaillée disponible dans ce gist](https://gist.github.com/FredM67/986e1cb0fc020fa6324ccc151006af99). Ce guide complet vous explique pas à pas comment configurer votre ESP32 avec ESPHome pour exploiter au maximum les fonctionnalités de votre PVRouter dans Home Assistant.

# Configuration sans carte d'extension

> [!IMPORTANT]
> Si vous ne disposez pas de la carte d'extension spécifique ni du PCB carte-mère approprié (ces deux éléments n'étant pas disponibles pour l'instant), vous pouvez toujours réaliser l'intégration par vos propres moyens.

Dans ce cas :
- Aucune connexion n'est prédéfinie entre l'ESP32 et le Mk2PVRouter
- Vous devrez réaliser votre propre câblage selon vos besoins
- Veillez à configurer de façon cohérente :
  - Le programme du routeur (fichier config.h)
  - La configuration ESPHome sur l'ESP32

Assurez-vous notamment que les numéros de pins utilisés dans chaque configuration correspondent exactement à vos connexions physiques. N'oubliez pas d'utiliser des adaptateurs de niveau logique si nécessaire entre le Mk2PVRouter (5 V) et l'ESP32 (3.3 V).

Pour les sondes de température, vous pouvez les connecter directement à l'ESP32 en utilisant une broche `GPIO` de votre choix, que vous configurerez ensuite dans ESPHome. **N'oubliez pas d'ajouter une résistance pull-up de 4,7 kΩ entre la ligne de données (DQ) et l'alimentation +3,3 V** pour assurer le bon fonctionnement du bus 1-Wire.

> [!NOTE]
> Même sans la carte d'extension, toutes les fonctionnalités d'intégration avec Home Assistant restent accessibles, à condition que votre câblage et vos configurations logicielles soient correctement réalisés.

Pour plus de détails sur la configuration d'ESPHome et l'intégration avec Home Assistant, consultez la [documentation détaillée disponible dans ce gist](https://gist.github.com/FredM67/986e1cb0fc020fa6324ccc151006af99). Ce guide complet vous explique pas à pas comment configurer votre ESP32 avec ESPHome pour exploiter au maximum les fonctionnalités de votre PVRouter dans Home Assistant.

# Dépannage
- Assurez-vous que toutes les bibliothèques requises sont installées.
- Vérifiez la configuration correcte des pins et des paramètres.
- Consultez la sortie série pour les messages d'erreur.

# Contribuer
Les contributions sont les bienvenues ! Veuillez soumettre des problèmes, des demandes de fonctionnalités et des pull requests via GitHub.
