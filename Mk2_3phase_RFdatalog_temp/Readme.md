[![en](https://img.shields.io/badge/lang-en-red.svg)](Readme.en.md)

Ce programme doit être utilisé avec l’IDE Arduino et/ou d’autres IDE de développement comme VSCode + PlatformIO.

- [Utilisation avec Arduino IDE](#utilisation-avec-arduino-ide)
- [Utilisation avec Visual Studio Code](#utilisation-avec-visual-studio-code)
- [Aperçu rapide des fichiers](#aperçu-rapide-des-fichiers)
- [Étalonnage du routeur](#étalonnage-du-routeur)
- [Configuration du programme](#configuration-du-programme)
  - [Configuration des sorties TRIAC](#configuration-des-sorties-triac)
  - [Configuration des sorties relais tout-ou-rien](#configuration-des-sorties-relais-tout-ou-rien)
    - [Principe de fonctionnement](#principe-de-fonctionnement)

# Utilisation avec Arduino IDE

Vous devrez télécharger et installer la version la plus récente de l'[Arduino IDE](https://www.arduino.cc/en/software).

Téléchargez la version « standard », PAS la version du Microsoft Store.
Procurez-vous la version « Win 10 et plus récent, 64 bits » ou la version « MSI installer ».

Étant donné que le code est optimisé avec l'une des dernières normes de C++, vous devrez modifier un fichier de configuration pour activer C++17.

Veuillez rechercher le fichier '**platform.txt**' situé dans le chemin d’installation de l’IDE Arduino.

Pour **Windows**, typiquement, vous trouverez le fichier dans '**C:\Program Files (x86)\Arduino\hardware\arduino\avr**' et/ou dans '**%LOCALAPPDATA%\Arduino15\packages\arduino\hardware\avr\x.y.z**' où 'x.y.z' est la version du package **Arduino AVR Boards**.

Vous pouvez aussi taper cette commande dans un Powershell : `Get-Childitem –Path C:\ -Include platform.txt -Recurse -ErrorAction SilentlyContinue`. Cela peut prendre quelques secondes/minutes jusqu’à ce que le fichier soit trouvé.

Pour **Linux**, si vous utilisez le paquet AppImage, vous trouverez ce fichier dans '**~/.arduino15/packages/arduino/hardware/avr/1.8.6**'.  
Vous pouvez exécuter `find / -name platform.txt 2>/dev/null` au cas où l’emplacement aurait été modifié.

Modifiez le fichier dans n’importe quel éditeur de texte (vous aurez besoin des **droits d’administrateur**) et remplacez le paramètre '**-std=gnu++11**' par '**-std=gnu++17**'. Voilà!	

Si votre IDE Arduino a été ouvert, veuillez fermer toutes les instances et l’ouvrir à nouveau.

# Utilisation avec Visual Studio Code

Vous devrez installer des extensions supplémentaires.  
Les extensions les plus populaires et les plus utilisées pour ce travail sont '*Arduino*' et '*Platform IO*'.

# Aperçu rapide des fichiers

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
Il faudra dans un 1<sup>er</sup> temps définir le nombre de sortie TRIAC.
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
**_Note_**
Attention au suffixe '**_i**' !

*doc non finie*