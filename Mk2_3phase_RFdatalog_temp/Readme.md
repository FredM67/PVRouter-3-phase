[![en](https://img.shields.io/badge/lang-en-red.svg)](Readme.en.md)

Ce programme doit être utilisé avec l’IDE Arduino et/ou d’autres IDE de développement comme VSCode + PlatformIO.

# Utilisation avec Arduino IDE

Vous devrez télécharger et installer la version la plus récente de l'[Arduino IDE](https://www.arduino.cc/en/software).

Téléchargez la version « standard », PAS la version du Microsoft Store.
Procurez-vous la version « Win 10 et plus récent, 64 bits » ou la version « MSI installer ».

Étant donné que le code est optimisé avec l'une des dernières normes de C++, vous devrez modifier un fichier de configuration pour activer C++17.

Veuillez rechercher le fichier '**platform.txt**' situé dans le chemin d’installation de l’IDE Arduino.

Pour **Windows**, typiquement, vous trouverez le fichier dans '**C:\Program Files (x86)\Arduino\hardware\arduino\avr**' et/ou dans '**%LOCALAPPDATA%\Arduino15\packages\arduino\hardware\avr\x.y.z**' où 'x.y.z' est la version du package **Arduino AVR Boards**.

Vous pouvez aussi taper cette commande dans un Powershell : `Get-Childitem –Path C:\ -Include platform.txt -Recurse -ErrorAction SilentlyContinue`. Cela peut prendre quelques secondes/minutes jusqu’à ce que le fichier soit trouvé.

Pour **Linux**, si vous utilisez le paquet AppImage, vous trouverez ce fichier dans '**~/.arduino15/packages/arduino/hardware/avr/1.8.6**'.  
Vous pouvez exécuter `find / -name platform.txt 2>/dev/null` au cas où l’emplacement aurait été modifié.

Modifiez le fichier dans n’importe quel éditeur de texte (vous aurez besoin des **droits d’administrateur**) et remplacez le paramètre '**-std=gnu++11**' par '**-std=gnu++17**'. Voilà!	

Si votre IDE Arduino a été ouvert, veuillez fermer toutes les instances et l’ouvrir à nouveau.

# Utilisation avec Visual Studio Code

Vous devrez installer des extensions supplémentaires. Les extensions les plus populaires et les plus utilisées pour ce travail sont '*Arduino*' et '*Platform IO*'.

# Aperçu rapide des fichiers

- **Mk2_3phase_RFdatalog_temp.ino** : Ce fichier est nécessaire pour l’IDE Arduino
- **calibration.h** : contient les paramètres d’étalonnage
- **config.h** : les préférences de l’utilisateur sont stockées ici (affectation des broches, fonctionnalités, ...)
- **config_system.h** : constantes système rarement modifiées
- **constants.h** : quelques constantes - *ne pas modifier*
- **debug.h** : Quelques macros pour la sortie série et le débogage
- **dualtariff.h** : définitions de la fonction double tarif
- **main.cpp** : code source principal
- **main.h** : prototypes de fonctions
- **movingAvg.h** : code source pour la moyenne glissante
- **processing.cpp** : code source du moteur de traitement
- **processing.h** : prototypes de fonctions du moteur de traitement
- **Readme.fr.md** : ce fichier
- **types.h** : définitions des types, ...
- **type_traits.h** : quelques trucs STL qui ne sont pas encore disponibles dans le paquet avr
- **type_traits** : contient des patrons STL manquants
- **utils_relay.h** : code source de la fonctionnalité *diversion par relais*
- **utils_rf.h** : code source de la fonction *RF*
- **utils_temp.h** : code source de la fonctionnalité *Température*
- **utils.h** : fonctions d’aide et trucs divers
- **validation.h** : validation des paramètres, ce code n’est exécuté qu’au moment de la compilation !
- **platformio.ini** : paramètres PlatformIO
- **inject_sketch_name.py** : script d'aide pour PlatformIO
- **Doxyfile** : paramètre pour Doxygen (documentation du code)

L’utilisateur final ne doit éditer QUE les fichiers **calibration.h** et **config.h**.