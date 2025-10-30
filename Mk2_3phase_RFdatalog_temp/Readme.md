[![en](https://img.shields.io/badge/lang-en-red.svg)](Readme.en.md)

Ce programme est conçu pour être utilisé avec l’IDE Arduino et/ou d’autres IDE de développement comme VSCode + PlatformIO.

# Table des matières
- [Table des matières](#table-des-matières)
- [Utilisation avec Visual Studio Code (recommandé)](#utilisation-avec-visual-studio-code-recommandé)
- [Utilisation avec Arduino IDE](#utilisation-avec-arduino-ide)
  - [Bibliothèques requises pour l'Arduino IDE](#bibliothèques-requises-pour-larduino-ide)
    - [Bibliothèques obligatoires](#bibliothèques-obligatoires)
    - [Note importante](#note-importante)
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
  - [Configuration du module RF et des charges distantes](#configuration-du-module-rf-et-des-charges-distantes)
    - [Matériel requis](#matériel-requis)
    - [Configuration logicielle](#configuration-logicielle)
    - [Configuration du récepteur distant](#configuration-du-récepteur-distant)
  - [Configuration du Watchdog](#configuration-du-watchdog)
  - [Configuration du ou des capteurs de température](#configuration-du-ou-des-capteurs-de-température)
    - [Activation de la fonctionnalité](#activation-de-la-fonctionnalité)
      - [Avec l’Arduino IDE](#avec-larduino-ide)
      - [Avec Visual Studio Code et PlatformIO](#avec-visual-studio-code-et-platformio)
    - [Configuration du ou des capteurs (commun aux 2 cas précédents)](#configuration-du-ou-des-capteurs-commun-aux-2-cas-précédents)
  - [Configuration de la gestion des Heures Creuses (dual tariff)](#configuration-de-la-gestion-des-heures-creuses-dual-tariff)
    - [Configuration matérielle](#configuration-matérielle)
    - [Configuration logicielle](#configuration-logicielle-1)
  - [Rotation des priorités](#rotation-des-priorités)
  - [Configuration de la marche forcée (nouveau)](#configuration-de-la-marche-forcée-nouveau)
    - [Activation de la fonctionnalité](#activation-de-la-fonctionnalité-1)
    - [Définition des OverridePins](#définition-des-overridepins)
    - [Utilisation](#utilisation)
    - [Exemples de configuration](#exemples-de-configuration)
  - [Arrêt du routage](#arrêt-du-routage)
- [Configuration avancée du programme](#configuration-avancée-du-programme)
  - [Paramètre `DIVERSION_START_THRESHOLD_WATTS`](#paramètre-diversion_start_threshold_watts)
  - [Paramètre `REQUIRED_EXPORT_IN_WATTS`](#paramètre-required_export_in_watts)
- [Configuration avec la carte d’extension ESP32](#configuration-avec-la-carte-dextension-esp32)
  - [Correspondance des broches](#correspondance-des-broches)
  - [Configuration du pont `TEMP`](#configuration-du-pont-temp)
  - [Configuration recommandée](#configuration-recommandée)
    - [Configuration de base recommandée](#configuration-de-base-recommandée)
    - [Fonctionnalités additionnelles recommandées](#fonctionnalités-additionnelles-recommandées)
    - [Installation des sondes de température](#installation-des-sondes-de-température)
  - [Liaison avec Home Assistant](#liaison-avec-homeassistant)
- [Configuration sans carte d’extension](#configuration-sans-carte-dextension)
- [Dépannage](#dépannage)
- [Contribuer](#contribuer)

# Utilisation avec Visual Studio Code (recommandé)

Vous devrez installer des extensions supplémentaires. Les extensions les plus populaires et les plus utilisées pour ce travail sont '*Platform IO*' et '*Arduino*'.  
L’ensemble du projet a été conçu pour être utilisé de façon optimale avec *Platform IO*.

# Utilisation avec Arduino IDE

Pour utiliser ce programme avec l’IDE Arduino, vous devez télécharger et installer la dernière version de l’IDE Arduino. Choisissez la version "standard", PAS la version du Microsoft Store. Optez pour la version "Win 10 et plus récent, 64 bits" ou la version "MSI installer".

Comme le code est optimisé avec l’une des dernières normes C++, vous devez modifier un fichier de configuration pour activer C++17. Vous trouverez le fichier '**platform.txt**' dans le chemin d’installation de l’IDE Arduino.

Pour **Windows**, vous trouverez généralement le fichier dans '**C:\Program Files (x86)\Arduino\hardware\arduino\avr**' et/ou dans '**%LOCALAPPDATA%\Arduino15\packages\arduino\hardware\avr\x.y.z**' où **'x.y.z**' est la version du package Arduino AVR Boards.

Vous pouvez également exécuter cette commande dans Powershell : `Get-Childitem –Path C:\ -Include platform.txt -Recurse -ErrorAction SilentlyContinue`. Cela peut prendre quelques secondes/minutes jusqu’à ce que le fichier soit trouvé.

Pour **Linux**, si vous utilisez le package AppImage, vous trouverez ce fichier dans '~/.arduino15/packages/arduino/hardware/avr/1.8.6'. Vous pouvez exécuter `find / -name platform.txt 2>/dev/null` au cas où l’emplacement aurait changé.

Pour **MacOSX**, ce fichier se trouve dans '/Users/[user]/Library/Arduino15/packages/arduino/hardware/avr/1.8.6'.

Ouvrez le fichier dans n’importe quel éditeur de texte (vous aurez besoin des droits d’administrateur) et remplacez le paramètre '**-std=gnu++11**' par '**-std=gnu++17**'. C’est tout !

Si votre IDE Arduino était ouvert, veuillez fermer toutes les instances et le rouvrir.

## Bibliothèques requises pour l'Arduino IDE

Ce projet nécessite l'installation des bibliothèques suivantes via le **Gestionnaire de bibliothèques** de l'Arduino IDE (menu **Outils** → **Gérer les bibliothèques…**) :

### Bibliothèques obligatoires
- **OneWire** par Jim Studt et al. (version 2.3.7 ou supérieure)
  - Utilisée pour les capteurs de température DS18B20
  - Installée même si aucun capteur n'est utilisé (le code non utilisé sera éliminé par le linker)

- **RFM69** par Felix Rusu, LowPowerLab (version 1.5.3 ou supérieure)
  - Utilisée pour la communication RF (télémétrie et charges distantes)
  - Installée même si le module RF n'est pas présent (le code non utilisé sera éliminé par le linker)

- **ArduinoJson** par Benoit Blanchon (version **6.x uniquement**, PAS la 7.x)
  - Utilisée pour la sortie série en format JSON (dans `utils.h`)
  - La version 7.x est trop volumineuse pour un ATmega328P

- **SPI** (incluse avec l'Arduino IDE)
  - Utilisée pour la communication avec le module RFM69

### Note importante
Toutes les bibliothèques sont toujours incluses dans le code source. Cependant, seul le code réellement utilisé par votre configuration sera présent dans le firmware final. Cela simplifie la maintenance du code tout en préservant la taille du firmware.

**Avec PlatformIO** : Toutes les dépendances sont gérées automatiquement via le fichier `platformio.ini`. Aucune installation manuelle n'est nécessaire.
___
> [!WARNING]
> En cas d'utilisation de la libraire **ArduinoJson**, il faudra impérativement installer une version **6.x**.
> La version 7.x, certes plus actuelle, est devenue trop lourde pour un Atmega328P.
___

# Aperçu rapide des fichiers

- **Mk2_3phase_RFdatalog_temp.ino** : Ce fichier est nécessaire pour l’IDE Arduino
- **calibration.h** : contient les paramètres d’étalonnage
- **config.h** : les préférences de l’utilisateur sont stockées ici (affectation des broches, fonctionnalités …)
- **config_system.h** : constantes système rarement modifiées
- **constants.h** : quelques constantes — *ne pas modifier*
- **debug.h** : Quelques macros pour la sortie série et le débogage
- **dualtariff.h** : définitions de la fonction double tarif
- **ewma_avg.h** : fonctions de calcul de moyenne EWMA
- **main.cpp** : code source principal
- **movingAvg.h** : code source pour la moyenne glissante
- **processing.cpp** : code source du moteur de traitement
- **processing.h** : prototypes de fonctions du moteur de traitement
- **Readme.md** : ce fichier
- **teleinfo.h**: code source de la fonctionnalité *Télémétrie IoT*
- **types.h** : définitions des types …
- **type_traits.h** : quelques trucs STL qui ne sont pas encore disponibles dans le paquet avr
- **type_traits** : contient des patrons STL manquants
- **utils_dualtariff.h** : code source de la fonctionnalité *gestion Heures Creuses*
- **utils_pins.h** : quelques fonctions d’accès direct aux entrées/sorties du micro-contrôleur
- **utils_relay.h** : code source de la fonctionnalité *diversion par relais*
- **utils_rf.h** : code source de la fonction *RF*
- **utils_temp.h** : code source de la fonctionnalité *Température*
- **utils.h** : fonctions d’aide et trucs divers
- **validation.h** : validation des paramètres, ce code n’est exécuté qu’au moment de la compilation !
- **platformio.ini** : paramètres PlatformIO
- **inject_sketch_name.py** : script d’aide pour PlatformIO
- **Doxyfile** : paramètre pour Doxygen (documentation du code)

L’utilisateur final ne doit éditer QUE les fichiers **calibration.h** et **config.h**.

## Documentation technique
Le dossier **[docs/](docs/)** contient la documentation technique détaillée :
- **[Architecture logicielle](docs/architecture.md)** - Conception et organisation des modules
- **[Performances](docs/performance.md)** - Analyses de timing et optimisations

# Documentation de développement

Vous pouvez commencer à lire la documentation ici [3-phase routeur](https://fredm67.github.io/Mk2PVRouter-3-phase/) (en anglais).

# Étalonnage du routeur
Les valeurs d’étalonnage se trouvent dans le fichier **calibration.h**.
Il s’agit de la ligne :
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

La configuration d’une fonctionnalité suit généralement deux étapes :
- Activation de la fonctionnalité
- Configuration des paramètres de la fonctionnalité

La cohérence de la configuration est vérifiée lors de la compilation. Par exemple, si une *pin* est allouée deux fois par erreur, le compilateur générera une erreur.

## Type de sortie série

Le type de sortie série peut être configuré pour s’adapter à différents besoins. Trois options sont disponibles :

- **HumanReadable** : Sortie lisible par un humain, idéale pour le débogage ou la mise en service.
- **IoT** : Sortie formatée pour des plateformes IoT comme Home Assistant.
- **JSON** : Sortie formatée pour des plateformes comme EmonCMS (JSON).

Pour configurer le type de sortie série, modifiez la constante suivante dans le fichier **config.h** :
```cpp
inline constexpr SerialOutputType SERIAL_OUTPUT_TYPE = SerialOutputType::HumanReadable;
```
Remplacez `HumanReadable` par `IoT` ou `JSON` selon vos besoins.

## Configuration des sorties TRIAC

La première étape consiste à définir le nombre de sorties TRIAC :

```cpp
inline constexpr uint8_t NO_OF_DUMPLOADS{ 2 };
```

Ensuite, il faudra assigner les *pins* correspondantes **uniquement pour les charges locales** ainsi que l'ordre des priorités au démarrage.
```cpp
// Pins pour les charges LOCALES uniquement (les charges distantes sont contrôlées via RF)
inline constexpr uint8_t physicalLoadPin[NO_OF_DUMPLOADS - NO_OF_REMOTE_LOADS]{ 5 };

// Optionnel : LEDs d'état pour les charges distantes
inline constexpr uint8_t remoteLoadStatusLED[NO_OF_REMOTE_LOADS]{ unused_pin, unused_pin };

// Ordre de priorités au démarrage (0 = priorité la plus haute, s'applique à TOUTES les charges)
inline constexpr uint8_t loadPrioritiesAtStartup[NO_OF_DUMPLOADS]{ 0, 1 };
```

**Important :** 
- `physicalLoadPin` ne contient que les pins des charges **locales** (TRIACs connectés directement)
- Les charges **distantes** n'ont pas de pin physique sur le contrôleur principal (elles sont contrôlées via RF)
- `remoteLoadStatusLED` permet optionnellement d'ajouter des LEDs d'état pour visualiser l'état des charges distantes
- `loadPrioritiesAtStartup` définit l'ordre de priorité pour **toutes** les charges (locales + distantes). Les priorités 0 à (nombre de charges locales - 1) contrôlent les charges locales, les priorités suivantes contrôlent les charges distantes.

## Configuration des sorties relais tout-ou-rien
Les sorties relais tout-ou-rien permettent d’alimenter des appareils qui contiennent de l’électronique (pompe à chaleur …).

Il faudra activer la fonctionnalité comme ceci :
```cpp
inline constexpr bool RELAY_DIVERSION{ true };
```

Chaque relais nécessite la définition de cinq paramètres :
- le numéro de **pin** sur laquelle est branché le relais
- le **seuil de surplus** avant mise en route (par défaut **1000 W**)
- le **seuil d’import** avant arrêt (par défaut **200 W**)
- la **durée de fonctionnement minimale** en minutes (par défaut **5 min**)
- la **durée d’arrêt minimale** en minutes (par défaut **5 min**).

Exemple de configuration d’un relais :
```cpp
inline constexpr RelayEngine relays{ { { 4, 1000, 200, 10, 10 } } };
```
Dans cet exemple, le relais est connecté sur la *pin* **4**, il se déclenchera à partir de **1000 W** de surplus, s’arrêtera à partir de **200 W** d’import, et a une durée minimale de fonctionnement et d’arrêt de **10 min**.

Pour configurer plusieurs relais, listez simplement les configurations de chaque relais :
```cpp
inline constexpr RelayEngine relays{ { { 4, 1000, 200, 10, 10 },
                                       { 3, 1500, 250, 5, 15 } } };
```
Les relais sont activés dans l’ordre de la liste, et désactivés dans l’ordre inverse.  
Dans tous les cas, les durées minimales de fonctionnement et d’arrêt sont toujours respectées.

### Principe de fonctionnement
Les seuils de surplus et d’import sont calculés en utilisant une moyenne mobile pondérée exponentiellement (EWMA), dans notre cas précis, il s’agit d’une modification d’une moyenne mobile triple exponentiellement pondérée (TEMA).  
Par défaut, cette moyenne est calculée sur une fenêtre d’environ **10 min**. Vous pouvez ajuster cette durée pour l’adapter à vos besoins.  
Il est possible de la rallonger mais aussi de la raccourcir.  
Pour des raisons de performances de l’Arduino, la durée choisie sera arrondie à une durée proche qui permettra de faire les calculs sans impacter les performances du routeur.

La durée de la fenêtre temporelle est contrôlée par le paramètre `RELAY_FILTER_DELAY` dans le fichier de configuration.

Si l’utilisateur souhaite plutôt une fenêtre de 15 min, il suffira d’écrire :
```cpp
inline constexpr RelayEngine relays{ MINUTES(15), { { 3, 1000, 200, 1, 1 } } };
```
___
> [!NOTE]
> La macro `MINUTES()` convertit automatiquement la valeur en paramètre template. Aucun suffixe spécial n'est nécessaire !
___

Les relais configurés dans le système sont gérés par un système similaire à une machine à états.
Chaque seconde, le système augmente la durée de l’état actuel de chaque relais et procède avec tous les relais en fonction de la puissance moyenne actuelle :
- si la puissance moyenne actuelle est supérieure au seuil d’import, elle essaie d’éteindre certains relais.
- si la puissance moyenne actuelle est supérieure au seuil de surplus, elle essaie d’allumer plus de relais.

Les relais sont traités dans l’ordre croissant pour le surplus et dans l’ordre décroissant pour l’importation.

Pour chaque relais, la transition ou le changement d’état est géré de la manière suivante :
- si le relais est *OFF* et que la puissance moyenne actuelle est inférieure au seuil de surplus, le relais essaie de passer à l’état *ON*. Cette transition est soumise à la condition que le relais ait été *OFF* pendant au moins la durée *minOFF*.
- si le relais est *ON* et que la puissance moyenne actuelle est supérieure au seuil d’importation, le relais essaie de passer à l’état *OFF*. Cette transition est soumise à la condition que le relais ait été *ON* pendant au moins la durée *minON*.

> [!NOTE]
> **Installations avec batteries :** Pour une configuration optimale des relais avec systèmes de batteries, consultez le **[Guide de Configuration pour Systèmes Batterie](docs/BATTERY_CONFIGURATION_GUIDE.md)** [![en](https://img.shields.io/badge/lang-en-red.svg)](docs/BATTERY_CONFIGURATION_GUIDE.en.md)

## Configuration du module RF et des charges distantes

Le routeur peut contrôler des charges distantes via un module RF RFM69. Cette fonctionnalité permet de piloter des résistances ou des relais situés dans un autre emplacement, sans câblage supplémentaire.

### Matériel requis

**Pour l'émetteur (routeur principal) :**
- Module RFM69W/CW ou RFM69HW/HCW (868 MHz pour l'Europe, 915 MHz pour l'Amérique du Nord)
- Antenne appropriée pour la fréquence choisie
- Connexion SPI standard (D10=CS, D2=IRQ)

**Pour le récepteur distant :**
- Arduino UNO ou compatible
- Module RFM69 (même modèle que l'émetteur)
- TRIAC ou SSR pour commander les charges
- LEDs optionnelles pour indication d'état (D5=verte watchdog, D7=rouge perte RF)

### Configuration logicielle

**Activation des fonctionnalités RF :**

Le module RF peut être utilisé pour deux fonctionnalités indépendantes :

1. **Télémétrie RF** (`ENABLE_RF_DATALOGGING`) : Envoi des données de puissance/tension vers une passerelle
2. **Charges distantes** (`ENABLE_REMOTE_LOADS`) : Contrôle de charges via RF

Pour activer le module RF avec contrôle de charges distantes, décommentez dans **config.h** :

```cpp
#define RF_PRESENT                /**< Active le module RFM69 (requis pour toute fonction RF) */
#define ENABLE_REMOTE_LOADS       /**< Active le contrôle de charges distantes via RF */
```

**Configuration des charges :**

Définissez le nombre total de charges (locales + distantes) :

```cpp
inline constexpr uint8_t NO_OF_DUMPLOADS{ 3 };        // Total : 3 charges
inline constexpr uint8_t NO_OF_REMOTE_LOADS{ 2 };     // Dont 2 charges distantes
                                                       // Charges locales : 3 - 2 = 1

// Pin pour la charge locale (TRIAC)
inline constexpr uint8_t physicalLoadPin[NO_OF_DUMPLOADS - NO_OF_REMOTE_LOADS]{ 5 };

// LEDs optionnelles pour indiquer l'état des charges distantes
inline constexpr uint8_t remoteLoadStatusLED[NO_OF_REMOTE_LOADS]{ 8, 9 };  // D8 et D9
```

**Priorités :**

Les charges distantes ont **toujours** une priorité inférieure aux charges locales. Dans l'exemple ci-dessus :
- Charge locale #0 (physicalLoadPin[0]) : priorité la plus haute
- Charge distante #0 : priorité moyenne  
- Charge distante #1 : priorité la plus basse

**Configuration RF (dans utils_rf.h) :**

Les paramètres par défaut sont :
- Fréquence : 868 MHz (Europe)
- ID réseau : 210
- ID émetteur : 10
- ID récepteur : 15

Pour modifier ces paramètres, éditez **utils_rf.h** :

```cpp
inline constexpr uint8_t THIS_NODE_ID{ 10 };        // ID de cet émetteur
inline constexpr uint8_t GATEWAY_ID{ 1 };           // ID de la passerelle (télémétrie)
inline constexpr uint8_t REMOTE_LOAD_ID{ 15 };     // ID du récepteur de charges
inline constexpr uint8_t NETWORK_ID{ 210 };        // ID du réseau (1-255)
```

### Configuration du récepteur distant

Le sketch **RemoteLoadReceiver** est fourni dans le dossier `RemoteLoadReceiver/`.

**Configuration minimale (dans config.h du récepteur) :**

```cpp
// Configuration RF - doit correspondre à l'émetteur
inline constexpr uint8_t TX_NODE_ID{ 10 };          // ID de l'émetteur
inline constexpr uint8_t MY_NODE_ID{ 15 };          // ID de ce récepteur
inline constexpr uint8_t NETWORK_ID{ 210 };         // ID réseau

// Configuration des charges
inline constexpr uint8_t NO_OF_LOADS{ 2 };                    // Nombre de charges sur ce récepteur
inline constexpr uint8_t loadPins[NO_OF_LOADS]{ 4, 3 };       // Pins des sorties TRIAC/SSR

// LEDs d'état (optionnel)
inline constexpr uint8_t GREEN_LED_PIN{ 5 };        // LED verte : watchdog 1 Hz
inline constexpr uint8_t RED_LED_PIN{ 7 };          // LED rouge : perte liaison RF (clignotement rapide)
inline constexpr bool STATUS_LEDS_PRESENT{ true };  // Activer les LEDs
```

**Sécurité :**

Le récepteur désactive automatiquement **toutes les charges** si aucun message n'est reçu pendant plus de 500 ms. Cela garantit la sécurité en cas de perte de liaison RF.

**Test de la liaison :**

Une fois configurés et téléversés, les deux Arduino communiquent automatiquement :
- L'émetteur envoie l'état des charges toutes les ~100 ms (5 cycles secteur à 50 Hz)
- Le récepteur affiche les commandes reçues sur le port série
- La LED verte clignote à 1 Hz (système actif)
- La LED rouge clignote rapidement si la liaison RF est perdue

**Diagnostic :**

Sur le moniteur série du récepteur, vous devriez voir :
```
Received: 0b01 (RSSI: -45) - Loads: 0:ON 1:OFF
```

Un RSSI entre -30 et -70 indique une bonne qualité de signal. Au-delà de -80, la liaison devient instable.

## Configuration du Watchdog
Un chien de garde, en anglais *watchdog*, est un circuit électronique ou un logiciel utilisé en électronique numérique pour s’assurer qu’un automate ou un ordinateur ne reste pas bloqué à une étape particulière du traitement qu’il effectue.

Ceci est réalisé à l’aide d’une LED qui clignote à la fréquence de 1 Hz, soit toutes les secondes.  
Ainsi, l’utilisateur sait d’une part si son routeur est allumé, et si jamais cette LED ne clignote plus, c’est que l’Arduino s’est bloqué (cas encore jamais rencontré !).  
Un simple appui sur le bouton *Reset* permettra de redémarrage le système sans rien débrancher.

Il faudra activer la fonctionnalité comme ceci :
```cpp
inline constexpr bool WATCHDOG_PIN_PRESENT{ true };
```
et définir la *pin* utilisée, dans l’exemple la *9* :
```cpp
inline constexpr uint8_t watchDogPin{ 9 };
```

## Configuration du ou des capteurs de température
Il est possible de brancher un ou plusieurs capteurs de température Dallas DS18B20.  
Ces capteurs peuvent servir à des fins informatives ou pour contrôler le mode de fonctionnement forcé.

Pour activer cette fonctionnalité, il faudra procéder différemment selon que l’on utilise l’Arduino IDE ou Visual Studio Code avec l’extension PlatformIO.

Par défaut, la sortie `D3` est utilisée pour la sortie du capteur de température et dispose déjà d’un pull-up.  
Si vous souhaitez utiliser une autre pin, il faudra rajouter un *pull-up* sur la pin utilisée.

### Activation de la fonctionnalité

Pour activer cette fonctionnalité, la procédure diffère selon que vous utilisez l’Arduino IDE ou Visual Studio Code avec l’extension PlatformIO.

#### Avec l’Arduino IDE
Activez la ligne suivante en supprimant le commentaire :
```cpp
#define TEMP_ENABLED
```

Si la bibliothèque *OneWire* n’est pas installée, installez-la via le menu **Outils** => **Gérer les bibliothèques…**.  
Recherchez "Onewire" et installez "**OneWire** par Jim Studt, …" en version **2.3.7** ou plus récente.

#### Avec Visual Studio Code et PlatformIO
Sélectionnez la configuration "**env:temperature (Mk2_3phase_RFdatalog_temp)**".

### Configuration du ou des capteurs (commun aux 2 cas précédents)
Pour configurer les capteurs, vous devez entrer leurs adresses.  
Utilisez un programme pour scanner les capteurs connectés.  
Vous pouvez trouver de tels programmes sur Internet ou parmi les exemples fournis avec l’Arduino IDE.  
Il est recommandé de coller une étiquette avec l’adresse de chaque capteur sur son câble.

Entrez les adresses comme suit :
```cpp
inline constexpr TemperatureSensing temperatureSensing{ 4,
                                                        { { 0x28, 0xBE, 0x41, 0x6B, 0x09, 0x00, 0x00, 0xA4 },
                                                          { 0x28, 0x1B, 0xD7, 0x6A, 0x09, 0x00, 0x00, 0xB7 } } };
```
Le nombre *4* en premier paramètre est la *pin* que l’utilisateur aura choisi pour le bus *OneWire*.

___
> [!NOTE]
> Plusieurs capteurs peuvent être branchés sur le même câble.
> Sur Internet vous trouverez tous les détails concernant la topologie utilisable avec ce genre de capteurs.
___

## Configuration de la gestion des Heures Creuses (dual tariff)
Il est possible de confier la gestion des Heures Creuses au routeur.  
Cela permet par exemple de limiter la chauffe en marche forcée afin de ne pas trop chauffer l’eau dans l’optique d’utiliser le surplus le lendemain matin.  
Cette limite peut être en durée ou en température (nécessite d’utiliser un capteur de température Dallas DS18B20).

### Configuration matérielle
Décâblez la commande du contacteur Jour/Nuit, qui n’est plus nécessaire.  
Reliez directement une *pin* choisie au contact sec du compteur (bornes *C1* et *C2*).
___
> [!WARNING]
> Il faut relier **directement**, une paire *pin/masse* avec les bornes *C1/C2* du compteur.
> Il NE doit PAS y avoir de 230 V sur ce circuit !
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

Configurez la durée en *heures* de la période d’Heures Creuses (pour l’instant, une seule période est supportée par jour) :
```cpp
inline constexpr uint8_t ul_OFF_PEAK_DURATION{ 8 };
```

Enfin, on définira les modalités de fonctionnement pendant la période d’Heures Creuses :
```cpp
inline constexpr pairForceLoad rg_ForceLoad[NO_OF_DUMPLOADS]{ { -3, 2 } };
```
Il est possible de définir une configuration pour chaque charge indépendamment l’une des autres.
Le premier paramètre de *rg_ForceLoad* détermine la temporisation de démarrage par rapport au début ou à la fin des Heures Creuses :
- si le nombre est positif et inférieur à 24, il s’agit du nombre d’heures,
- si le nombre est négatif supérieur à −24, il s’agit du nombre d’heures par rapport à la fin des Heures Creuses
- si le nombre est positif et supérieur à 24, il s’agit du nombre de minutes,
- si le nombre est négatif inférieur à −24, il s’agit du nombre de minutes par rapport à la fin des Heures Creuses

Le deuxième paramètre détermine la durée de la marche forcée :
- si le nombre est inférieur à 24, il s’agit du nombre d’heures,
- si le nombre est supérieur à 24, il s’agit du nombre de minutes.

Exemples pour mieux comprendre (avec début d’HC à 23:00, jusqu’à 7:00 soit 8 h de durée) :
- ```{ -3, 2 }``` : démarrage **3 heures AVANT** la fin de période (à 4 h du matin), pour une durée de 2 h.
- ```{ 3, 2 }``` : démarrage **3 heures APRÈS** le début de période (à 2 h du matin), pour une durée de 2 h.
- ```{ -150, 2 }``` : démarrage **150 minutes AVANT** la fin de période (à 4:30), pour une durée de 2 h.
- ```{ 3, 180 }``` : démarrage **3 heures APRÈS** le début de période (à 2 h du matin), pour une durée de 180 min.

Pour une durée *infinie* (donc jusqu’à la fin de la période d’HC), utilisez ```UINT16_MAX``` comme deuxième paramètre :
- ```{ -3, UINT16_MAX }``` : démarrage **3 heures AVANT** la fin de période (à 4 h du matin) avec marche forcée jusqu’à la fin de période d’HC.

Si votre système est constitué 2 sorties (```NO_OF_DUMPLOADS``` aura alors une valeur de 2), et que vous souhaitez une marche forcée uniquement sur la 2ᵉ sortie, écrivez :
```cpp
inline constexpr pairForceLoad rg_ForceLoad[NO_OF_DUMPLOADS]{ { 0, 0 },
                                                              { -3, 2 } };
```

## Rotation des priorités
La rotation des priorités est utile lors de l’alimentation d’un chauffe-eau triphasé.  
Elle permet d’équilibrer la durée de fonctionnement des différentes résistances sur une période prolongée.

Mais elle peut aussi être intéressante si on veut permuter les priorités de deux appareils chaque jour (deux chauffe-eau, …).

Une fois n’est pas coutume, l’activation de cette fonction possède 2 modes :
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

## Configuration de la marche forcée (nouveau)

La marche forcée (*Boost*) peut désormais être déclenchée via une ou plusieurs *pins*, avec une association flexible entre chaque pin et les charges (dump loads) ou relais à activer. Cette fonctionnalité permet :

- D’activer la marche forcée depuis plusieurs emplacements ou dispositifs
- De cibler précisément une ou plusieurs charges ou relais pour chaque pin
- De grouper plusieurs charges/relais sous une même commande

### Activation de la fonctionnalité

Activez la fonctionnalité dans votre configuration :
```cpp
inline constexpr bool OVERRIDE_PIN_PRESENT{ true };
```

### Définition des OverridePins

La structure `OverridePins` permet d’associer chaque pin à une ou plusieurs charges ou relais, ou à des groupes prédéfinis (par exemple « toutes les charges », « tous les relais », ou « tout le système »).

Chaque entrée du tableau correspond à une pin, suivie d’une liste ou d’une fonction spéciale qui permet d’activer un ou plusieurs groupes de charges ou relais lors de la marche forcée.

Exemples :
```cpp
// Méthode classique : liste d’indices ou macros LOAD/RELAY
inline constexpr OverridePins overridePins{
  {
    { 2, { 1, LOAD(1) } },       // Pin 2 active la charge ou le relais connecté·e à la pin 1 et la charge #1
    { 4, { LOAD(0), RELAY(0) } } // Pin 4 active le charge #0 et le relais #0
  }
};

// Méthode avancée : bitmask pour tous les loads ou tous les relais
inline constexpr OverridePins overridePins{
  {
    { 2, ALL_LOADS() },           // Pin 2 active toutes les charges
    { 3, ALL_RELAYS() },          // Pin 3 active tous les relais
    { 4, ALL_LOADS_AND_RELAYS() } // Pin 4 active tout le système
  }
};
```
- `LOAD(n)` : référence le numéro de la charge (résistance pilotée, 0 → charge #1)
- `RELAY(n)` : référence le numéro de relais (sortie relais tout-ou-rien, 0 → relais #1)
- `ALL_LOADS()` : toutes les charges
- `ALL_RELAYS()` : tous les relais
- `ALL_LOADS_AND_RELAYS()` : tout le système (charges et relais)

**Groupement :**  
Plusieurs charges ou relais peuvent être groupés sous une même pin, soit en les listant, soit en utilisant les fonctions spéciales pour tout activer d’un coup.
Plusieurs pins peuvent piloter des groupes différents ou partiellement recoupés.

### Utilisation

- Reliez chaque pin configurée à un contact sec (interrupteur, minuterie, automate, etc.)
- Lorsqu’un contact est fermé, toutes les charges/relais associées à cette pin passent en marche forcée
- Dès que tous les contacts sont ouverts, la marche forcée est désactivée

**Exemples d’usage :**
- Un bouton dans la salle de bain pour forcer le chauffe-eau uniquement
- Une minuterie sur une autre pin pour forcer tous les relais pendant 30 minutes
- Un automate domotique qui active plusieurs charges selon la demande

### Exemples de configuration

**Configuration simple :**
```cpp
// Pin 3 active la charge #0 et le relais #0
// Pin 4 active toutes les charges
inline constexpr OverridePins overridePins{ { { 3, { LOAD(0), RELAY(0) } },
                                              { 4, ALL_LOADS() } } };
```

**Configuration avancée :**
```cpp
// Configuration flexible avec groupes personnalisés
inline constexpr OverridePins overridePins{ { { 3, { RELAY(1), LOAD(1) } },     // Pin 3: charge #1 + relais #1
                                              { 4, ALL_LOADS() },              // Pin 4: toutes les charges
                                              { 11, { 1, LOAD(1), LOAD(2) } },  // Pin 11: charges spécifiques
                                              { 12, ALL_LOADS_AND_RELAYS() } } }; // Pin 12: tout le système
```

## Arrêt du routage
Il peut être pratique de désactiver le routage lors d’une absence prolongée.  
Cette fonctionnalité est particulièrement utile si la *pin* de commande est connectée à un contact sec qui peut être contrôlé à distance, par exemple via une routine Alexa ou similaire.  
Ainsi, vous pouvez désactiver le routage pendant votre absence et le réactiver un ou deux jours avant votre retour, afin de disposer d’eau chaude (gratuite) à votre arrivée.

Pour activer cette fonctionnalité, utilisez le code suivant :
```cpp
inline constexpr bool DIVERSION_PIN_PRESENT{ true };
```
Vous devez également spécifier la *pin* à laquelle le contact sec est connecté :
```cpp
inline constexpr uint8_t diversionPin{ 12 };
```

# Configuration avancée du programme

Ces paramètres se trouvent dans le fichier `config_system.h`.

## Paramètre `DIVERSION_START_THRESHOLD_WATTS`
Le paramètre `DIVERSION_START_THRESHOLD_WATTS` définit un seuil de surplus avant tout routage vers les charges configurées sur le routeur. Elle est principalement destinée aux installations avec batteries de stockage.   
Par défaut, cette valeur est réglée à 0 W.  
En réglant ce paramètre à 50 W par exemple, le routeur ne démarrera le routage qu'à partir du moment où 50 W de surplus sera disponible. Une fois le routage démarré, la totalité du surplus sera routé.  
Cette fonctionnalité permet d'établir une hiérarchie claire dans l’utilisation de l'énergie produite, en privilégiant le stockage d'énergie sur la consommation immédiate. Vous pouvez ajuster cette valeur selon la réactivité du système de charge des batteries et vos priorités d’utilisation de l'énergie.

> [!IMPORTANT]
> Ce paramètre concerne uniquement la condition de démarrage du routage.
> Une fois le seuil atteint et le routage démarré, la **totalité** du surplus devient disponible pour les charges.

## Paramètre `REQUIRED_EXPORT_IN_WATTS`
Le paramètre `REQUIRED_EXPORT_IN_WATTS` détermine la quantité minimale d'énergie que le système doit réserver pour l’exportation ou l’importation vers le réseau électrique avant de dévier le surplus vers les charges contrôlées.  
Par défaut réglé à 0 W, ce paramètre peut être utilisé pour garantir une exportation constante vers le réseau, par exemple pour respecter des accords de revente d'électricité.  
Une valeur négative obligera le routeur à consommer cette puissance depuis le réseau. Cela peut être utile voire nécessaire pour les installations configurées en *zéro injection* afin d’amorcer la production solaire.

> [!IMPORTANT]
> Contrairement au premier paramètre, celui-ci représente un décalage permanent qui est continuellement soustrait du surplus disponible.
> Si réglé à 20 W par exemple, le système réservera **toujours** 20 W pour l’exportation, indépendamment des autres conditions.

# Configuration avec la carte d’extension ESP32

La carte d’extension ESP32 permet une intégration simple et fiable entre le Mk2PVRouter et un ESP32 pour le contrôle à distance via Home Assistant. Cette section détaille comment configurer correctement le Mk2PVRouter lorsque vous utilisez cette carte d’extension.

## Correspondance des broches
Lorsque vous utilisez la carte d’extension ESP32, les connexions entre le Mk2PVRouter et l’ESP32 sont prédéfinies comme suit :

| ESP32  | Mk2PVRouter | Fonction                              |
| ------ | ----------- | ------------------------------------- |
| GPIO12 | D12         | Entrée/Sortie numérique - Usage libre |
| GPIO13 | D11         | Entrée/Sortie numérique - Usage libre |
| GPIO14 | D13         | Entrée/Sortie numérique - Usage libre |
| GPIO27 | D10         | Entrée/Sortie numérique - Usage libre |
| GPIO5  | DS18B20     | Bus 1-Wire pour sondes de température |

## Configuration du pont `TEMP`
**Important** : Si vous souhaitez que l’ESP32 contrôle les sondes de température (recommandé pour l’intégration avec Home Assistant), **le pont `TEMP` sur la carte mère du routeur ne doit pas être soudé**.
- **Pont `TEMP` non soudé** : L’ESP32 contrôle les sondes de température via GPIO5.
- **Pont `TEMP` soudé** : Le Mk2PVRouter contrôle les sondes de température via D3.

## Configuration recommandée
Pour une utilisation optimale avec Home Assistant, il est recommandé d’activer au minimum les fonctions suivantes :

### Configuration de base recommandée
```cpp
// Type de sortie série pour l’intégration IoT
inline constexpr SerialOutputType SERIAL_OUTPUT_TYPE = SerialOutputType::IoT;

// Fonctions essentielles recommandées
inline constexpr bool DIVERSION_PIN_PRESENT{ true };    // Arrêt du routage
inline constexpr bool OVERRIDE_PIN_PRESENT{ true };     // Marche forcée

// Pin configuration selon la correspondance de la carte d'extension
inline constexpr uint8_t diversionPin{ 12 };     // D12 - arrêt du routage

// Configuration de la marche forcée flexible
inline constexpr OverridePins overridePins{ { { 11, ALL_LOADS_AND_RELAYS() } } }; // D11 - marche forcée

// Configuration pour les sondes de température
// IMPORTANT: Désactiver la gestion de température dans le Mk2PVRouter
// si l’ESP32 gère les sondes (pont TEMP non soudé)
inline constexpr bool TEMP_SENSOR_PRESENT{ false };  // Désactivé car géré par l’ESP32
```

> [!NOTE]
> La configuration de la sortie série sur `SerialOutputType::IoT` n’est pas strictement obligatoire pour le fonctionnement du routeur. Cependant, elle est nécessaire si vous souhaitez exploiter les données du routeur dans Home Assistant (puissance instantanée, statistiques, etc.). Sans cette configuration, seules les fonctions de contrôle (marche forcée, arrêt routage) seront disponibles dans Home Assistant.

### Fonctionnalités additionnelles recommandées
Pour une intégration encore plus complète, vous pouvez également ajouter ces fonctionnalités :
```cpp
// Rotation des priorités via pin (optionnel)
inline constexpr RotationModes PRIORITY_ROTATION{ RotationModes::PIN };
inline constexpr uint8_t rotationPin{ 10 };      // D10 - rotation des priorités
```

### Installation des sondes de température
Pour l’installation des sondes de température :
- Assurez-vous que le pont `TEMP` n’est **pas** soudé sur la carte mère du routeur
- Connectez vos sondes DS18B20 directement via les connecteurs dédiés sur la carte mère du Mk2PVRouter
- Configurez les sondes dans ESPHome (aucune configuration n’est nécessaire côté Mk2PVRouter)

L’utilisation de l’ESP32 pour gérer les sondes de température présente plusieurs avantages :
- Visualisation des températures directement dans Home Assistant
- Possibilité de créer des automatisations basées sur les températures
- Configuration plus flexible des sondes sans avoir à reprogrammer le Mk2PVRouter

## Liaison avec Home Assistant
Une fois votre MkPVRouter configuré avec la carte d’extension ESP32, vous pourrez :
- Contrôler à distance l’activation/désactivation du routage (idéal pendant les absences)
- Déclencher une marche forcée à distance
- Surveiller les températures en temps réel
- Créer des scénarios d’automatisation avancés combinant les données de production solaire et les températures

Pour plus de détails sur la configuration d’ESPHome et l’intégration avec Home Assistant, consultez la [documentation détaillée disponible dans ce gist](https://gist.github.com/FredM67/986e1cb0fc020fa6324ccc151006af99). Ce guide complet vous explique pas à pas comment configurer votre ESP32 avec ESPHome pour exploiter au maximum les fonctionnalités de votre PVRouter dans Home Assistant.

# Configuration sans carte d’extension

> [!IMPORTANT]
> Si vous ne disposez pas de la carte d’extension spécifique ni du PCB carte-mère approprié (ces deux éléments n’étant pas disponibles pour l’instant), vous pouvez toujours réaliser l’intégration par vos propres moyens.

Dans ce cas :
- Aucune connexion n’est prédéfinie entre l’ESP32 et le Mk2PVRouter
- Vous devrez réaliser votre propre câblage selon vos besoins
- Veillez à configurer de façon cohérente :
  - Le programme du routeur (fichier config.h)
  - La configuration ESPHome sur l’ESP32
  
Assurez-vous notamment que les numéros de pins utilisés dans chaque configuration correspondent exactement à vos connexions physiques. N’oubliez pas d’utiliser des adaptateurs de niveau logique si nécessaire entre le Mk2PVRouter (5 V) et l’ESP32 (3.3 V).

Pour les sondes de température, vous pouvez les connecter directement à l’ESP32 en utilisant une broche `GPIO` de votre choix, que vous configurerez ensuite dans ESPHome. **N’oubliez pas d’ajouter une résistance pull-up de 4,7 kΩ entre la ligne de données (DQ) et l’alimentation +3,3 V** pour assurer le bon fonctionnement du bus 1-Wire.

> [!NOTE]
> Même sans la carte d’extension, toutes les fonctionnalités d’intégration avec Home Assistant restent accessibles, à condition que votre câblage et vos configurations logicielles soient correctement réalisés.

Pour plus de détails sur la configuration d’ESPHome et l’intégration avec Home Assistant, consultez la [documentation détaillée disponible dans ce gist](https://gist.github.com/FredM67/986e1cb0fc020fa6324ccc151006af99). Ce guide complet vous explique pas à pas comment configurer votre ESP32 avec ESPHome pour exploiter au maximum les fonctionnalités de votre PVRouter dans Home Assistant.

# Dépannage
- Assurez-vous que toutes les bibliothèques requises sont installées.
- Vérifiez la configuration correcte des pins et des paramètres.
- Consultez la sortie série pour les messages d’erreur.

# Contribuer
Les contributions sont les bienvenues ! Veuillez soumettre des problèmes, des demandes de fonctionnalités et des pull requests via GitHub.
