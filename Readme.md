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
  [![en](https://img.shields.io/badge/lang-en-red.svg)](https://github.com/FredM67/PVRouter-3-phase/blob/main/Readme.en.md)
  [![fr](https://img.shields.io/badge/lang-fr-blue.svg)](https://github.com/FredM67/PVRouter-3-phase/blob/main/Readme.md)
</div>

# PVRouter (version triphasée)

Ma version du firmware Mk2PVRouter en 3 phases (voir http://www.mk2pvrouter.co.uk).

Robin Emley propose déjà un routeur PV triphasé (https://www.mk2pvrouter.co.uk/3-phase-version.html).  
Il prend en charge jusqu'à 12 sorties pour charges résistives, qui sont complètement indépendantes.

---
**_NOTE:_** Pour une version en monophasé, voir [PVRouter-Single](https://github.com/FredM67/PVRouter-Single).

---

- [PVRouter (version triphasée)](#pvrouter-version-triphasée)
  - [Aperçu des dossiers](#aperçu-des-dossiers)
  - [Gallerie photo](#gallerie-photo)
  - [Schéma de la carte-mère](#schéma-de-la-carte-mère)
  - [Documentation de développement](#documentation-de-développement)
  - [Documentation de l’utilisateur final](#documentation-de-lutilisateur-final)
    - [Aperçu](#aperçu)
    - [Gestion des priorités de charge](#gestion-des-priorités-de-charge)
    - [Détection HC](#détection-hc)
    - [Marche forcée pleine puissance](#marche-forcée-pleine-puissance)
    - [Sortie(s) relais tout-ou-rien \*\* NEW \*\*](#sorties-relais-tout-ou-rien--new-)
    - [Capteur de température](#capteur-de-température)
    - [Profil Enphase zéro export](#profil-enphase-zéro-export)
  - [Comment câbler le routeur](#comment-câbler-le-routeur)
  - [Applications / Diagrammes de câblage](#applications--diagrammes-de-câblage)
    - [Pré-requis](#pré-requis)
    - [Chauffe-eau avec thermostat mécanique](#chauffe-eau-avec-thermostat-mécanique)
      - [Passage du monophasé au triphasé (avec neutre)](#passage-du-monophasé-au-triphasé-avec-neutre)
      - [Câblage](#câblage)
    - [Chauffe-eau avec thermostat ACI monophasé](#chauffe-eau-avec-thermostat-aci-monophasé)
    - [Chauffe-eau avec thermostat ACI triphasé (SANS neutre)](#chauffe-eau-avec-thermostat-aci-triphasé-sans-neutre)
    - [Alternatives SANS neutre](#alternatives-sans-neutre)
      - [Chauffe-eau avec thermostat mécanique](#chauffe-eau-avec-thermostat-mécanique-1)
  - [Support](#support)
  - [Roadmap](#roadmap)
  - [Contributing](#contributing)
  - [Authors and acknowledgment](#authors-and-acknowledgment)

## Aperçu des dossiers
- [**Mk2_3phase_RFdatalog_temp**](Mk2_3phase_RFdatalog_temp) : contient tous les fichiers nécessaires au programme du routeur.
- [**dev**](dev) : contient divers programmes pour le développement du routeur.
    - [**cal_CTx_v_meter**](dev/cal_CTx_v_meter) : contient tous les fichiers nécessaires au programme d'étalonnage du routeur.
    - **RawSamplesTool_6chan** : permet de tester les 6 canaux de mesure.
- autres dossiers : contiennent des fichiers divers et variés relatifs au site.

## Gallerie photo

Vous trouverez quelques [photos](Gallery.md) de routeurs assemblés.

## Schéma de la carte-mère

Vous trouverez [ici](schematics/3phase_Mainboard.pdf) le schéma de la carte-mère.

## Documentation de développement

Vous pouvez commencer à lire la documentation ici [3-phase diverter](https://fredm67.github.io/PVRouter-3-phase/) (en anglais).

## Documentation de l’utilisateur final

### Aperçu

L’objectif était de modifier/optimiser le programme pour le cas « spécial » d’un chauffe-eau triphasé. Un chauffe-eau triphasé est composé en fait de 3 éléments de chauffage indépendants. La plupart du temps, un tel chauffe-eau peut être connecté en monophasé, en triphasé étoile (WYE) ou triphasé triangle (Delta). Lorsqu’il est connecté en étoile (sans varistor), il n’y a pas besoin de fil de neutre parce que le système est équilibré, donc à tout moment, il n’y a pas de courant qui circule vers le neutre.

Fonctionnalités ajoutées :

- gestion des priorités de charge (configurable)
- détection HC/HP (configurable)
- forçage à pleine puissance
- capteur de température (juste la lecture pour le moment)
- enregistrement de données optimisé (RF)
- sortie série en JSON ou TXT
  
Le programme original a dû être entièrement retravaillé et re-structuré pour permettre la lecture de la température. Dans le programme d’origine, l’ISR ne fait que lire et convertir les données analogiques, et le traitement se fait dans la boucle *loop*. Cela ne fonctionnera pas avec un capteur de température en raison de ses performances lentes. Il déstabiliserait l’ensemble du système, des données de courant / tension seraient perdues, ...

Maintenant, tout le traitement critique en termes de temps se fait à l’intérieur de l’ISR, les autres tâches comme la journalisation des données (RF), la sortie série, la lecture de la température sont faites à l’intérieur de la boucle *loop()*. L’ISR et le processeur principal communiquent entre eux par le biais d'« événements ».

### Gestion des priorités de charge

Dans ma variante du programme de Robin, les 3 charges sont toujours physiquement indépendantes, c'est-à-dire que le routeur va détourner l’excédent d’énergie à la première charge (priorité la plus élevée) de 0% à 100%, puis à la seconde (0% à 100%) et enfin à la troisième.

Pour éviter que les priorités restent tout le temps inchangées, ce qui signifie que la charge 1 fonctionnera beaucoup plus que la charge 2, qui elle-même fonctionnera plus que la charge 3, j’ai ajouté une gestion des priorités. Chaque jour, les priorités des charges sont permutées, donc sur plusieurs jours, tous les éléments de chauffage fonctionneront en moyenne de façon équitable.

### Détection HC

Selon le pays, certains compteurs d’énergie disposent d'interrupteur/relais qui bascule au début de la période creuse. Il est destiné à contrôler un commutateur HC/HP. Si vous le reliez à une broche numérique libre du routeur (dans mon cas D3), vous pouvez détecter le début et fin des HC.

### Marche forcée pleine puissance

Le support a été ajouté pour forcer la pleine puissance sur des charges spécifiques. Chaque charge peut être forcée indépendamment les unes des autres, l’heure de début et la durée peuvent être définies individuellement.

Dans ma variante, c’est utilisé pour changer le chauffage pendant la période creuse, dans le cas où le surplus a été trop faible au cours de la journée. Ici, pour optimiser le comportement, un capteur de température sera utilisé pour vérifier la température de l’eau et décider d’allumer ou non pendant la nuit.

### Sortie(s) relais tout-ou-rien ** NEW **

Une ou plusieurs sorties tout-ou-rien via un relais peuvent être maintenant pilotées par le routeur.
Leur priorité sera toujours en dernier, c'est-à-dire que les sorties TRIAC hachées auront toujours une priorité plus élevée.

L'utilisateur devra définir pour cela, et ce pour chaque sortie relais :
- le seuil de surplus pour le déclenchement du relais (par défaut 1000W)
- le seuil d'import pour l'arrêt du relais (par défaut 200W)
- le temps minimal de fonctionnement du relais en minutes (par défaut 5 mn)
- le temps minimal d'arrêt du relais en minutes (par défaut 5 mn)

Les seuils de surplus et d'import sont calculés par une moyenne glissante sur une période de temps donnée. Par défaut, les moyennes sont calculées sur 1 minute.

### Capteur de température

Pour l’instant, uniquement lecture. Il sera utilisé pour optimiser la pleine puissance de la force, pour prendre la bonne décision pendant la nuit.

### Profil Enphase zéro export

Lorsque le profil zéro-export est activé, le système PV réduit la production d’énergie si la production du système dépasse les besoins de consommation du site. Cela garantit zéro injection dans le réseau.

Comme effet secondaire, le diverteur ne verra pas à aucun moment un surplus d’énergie.  
L’idée est donc d’appliquer un certain décalage à l’énergie mesurée par le diverteur.
Comme il est déjà commenté dans le code, après l'assignation d’une valeur négative à *REQUIRED_EXPORT_IN_WATTS*, le diverter agira comme un générateur PV.  
Si vous définissez une valeur de *-20*, chaque fois que le diverter mesure le flux d’énergie, il ajoutera *-20* aux mesures.  

Alors, maintenant voyons ce qui se passe dans différents cas:

- la valeur mesurée est **positive** (importation d’énergie = pas d’excédent), après avoir ajouté *-20*, cela reste positif, le diverter ne fait rien. Pour une valeur comprise entre -20 et 0, le déviateur ne fera rien non plus.
- la valeur mesurée est **autour de zéro**. Dans cette situation, la limitation du "profil zéro exportation" est active.  
Après l’ajout de *-20*, nous obtenons une valeur négative, ce qui déclenchera le détournement d’énergie vers le chauffe-eau.  
Ensuite, il y a une sorte de réaction en chaîne. L’Envoy détecte plus de consommation, décide d’augmenter la production.  
À la mesure suivante, le diverter mesure à nouveau une valeur autour de zéro, ajoute à nouveau -20, et détourne encore plus d’énergie.  
Lorsque la production (et l’excédent) arrive au maximum possible, la valeur mesurée restera autour de zéro+ et le système deviendra stable.

Cela a été testé en situation réelle par Amorim. Selon chaque situation, il peut être nécessaire de modifier cette valeur de *-20* à une valeur plus grande ou plus petite.

## Comment câbler le routeur
[Ici](docs/HowToInstall.pdf) vous trouverez une rapide notice d'installation du routeur.

## Applications / Diagrammes de câblage

Je veux:

- changer mon chauffe-eau (avec thermostat mécanique) monophasé en triphasé, voir [Chauffe-eau avec thermostat mécanique](#chauffe-eau-avec-thermostat-mécanique)
- connecter mon chauffe-eau (avec thermostat mécanique) en triphasé, voir [Chauffe-eau avec thermostat mécanique](#chauffe-eau-avec-thermostat-mécanique)
- changer mon chauffe-eau aci monophasé en triphasé sans acheter de kit triphasé, voir [Chauffe-eau avec thermostat ACI monophasé](#chauffe-eau-avec-thermostat-aci-monophasé)
- connecter mon chauffe-eau ACI triphasé, voir [Chauffe-eau avec thermostat ACI triphasé (SANS neutre)](#chauffe-eau-avec-thermostat-aci-triphasé-sans-neutre)
- connecter plusieurs charges résistives pures, il suffit de les câbler, une sur chaque sortie. N’oubliez pas de désactiver la gestion des priorités de charge.

### Pré-requis

Votre chauffe-eau DOIT supporter le câblage en triphasé (c'est-à-dire il doit y avoir 3 éléments chauffants).

---
**_Avertissement de sécurité_**

Pour modifier le câblage existant, l’accès à la tension du réseau 240V est nécessaire.  
Soyez sûr de savoir ce que vous entreprenez. Au besoin, faîtes appel à un électricien qualifié.

---

### Chauffe-eau avec thermostat mécanique

#### Passage du monophasé au triphasé (avec neutre)

---
**_Nécessite un routeur avec 3 sorties_**

Avec cette solution, vous commandez chaque résistance séparément l'une de l'autre.

---

Vous devrez séparer les 3 éléments de chauffage, et probablement ajouter un nouveau fil pour chacun d’eux. Parfois, les éléments sont reliés ensemble avec une sorte "d'étoile" métallique. Il y en a une pour la phase, et une pour le fil neutre. Vous n’avez qu’à supprimer celle de la phase, celle pour neutre doit rester câblée.

#### Câblage

Sur tous les chauffe-eau (triphasé) que j’ai vu, le thermostat ne coupe que 2 phases en mode normal (les 3 phases en mode de sécurité), il doit donc être câblé d’une autre manière pour obtenir une commutation complète sur les 3 phases.

---
**_Rappel_**

Dans une situation entièrement équilibrée en triphasé, vous n’avez pas besoin de fil neutre. Pour éteindre l’appareil, il suffit de couper 2 phases, ce qui explique la construction de ces thermostats

---

Pour cela, j’ai « recyclé » un commutateur HC/HP triphasé, mais vous pouvez utiliser n’importe quel relais triphasé. La bobine de commande doit être connectée à une alimentation "permanente" (et non à travers le routeur) contrôlée par le thermostat.

![Chauffe-eau avec thermostat mécanique](img/Heater_mechanical.png)  
*Figure: Diagramme de câblage*

### Chauffe-eau avec thermostat ACI monophasé

Dans ce cas, c’est en quelque sorte la même situation qu’avant. Vous n’avez pas besoin d’acheter un kit ACI en triphasé pour convertir votre chauffe-eau monophasé. La carte ACI doit être connectée à une phase permanente. Elle contrôlera ensuite n’importe quel relais en triphasé.

![Chauffe-eau avec thermostat ACI monophasé](img/Heater_ACI_Mono.png)  
*Figure : Diagramme de câblage*

### Chauffe-eau avec thermostat ACI triphasé (SANS neutre)

---
**_Nécessite un routeur avec 2 sorties_**

Avec cette solution, vous commandez chaque résistance séparément l'une de l'autre.

---

La carte ACI ne coupe pas les 3 phases lorsque la température est atteinte. Seules 2 phases sont coupées.

La phase non coupée est celle qui correspond au fil du milieu sur le connecteur. ***Il est très IMPORTANT que cette phase, non coupée par le thermostat, ne passe pas par un triac***.

La carte ACI doit être reliée à 3 phases permanentes.

![Chauffe-eau avec thermostat ACI triphasé](img/Heater_ACI_Tri.png)  
*Figure : Diagramme de câblage*

### Alternatives SANS neutre

---
**_Nécessite un routeur avec 2 sorties_**

Cette solution vous permet d'économiser le rajout d'un fil de neutre et/ou l'ajout un contacteur.

---

#### Chauffe-eau avec thermostat mécanique

Cette configuration permet de simplifier les branchements et surtout, il n'est plus nécessaire de rajouter un contacteur tri-/quadripolaire.

---
**_Zoom sur le thermostat_**

Il faut bien faire attention, en regardant sur le thermostat, quelles bornes sont coupées.

En **rouge**, coupure de sécurité (remarquez le 'S' sur chaque contact) : les 3 phases sont coupées.

En **vert**, seules 2 phases sont coupées, L2 et L3. ***Il est très IMPORTANT que la phase L1, non coupée par le thermostat, ne passe pas par un triac***.

![Thermostat mécanique](img/Thermostat.png)  
*Figure: Exemple de thermostat*

---

![Chauffe-eau avec thermostat mécanique](img/Heater_mechanical-No_neutral.png)  
*Figure: Diagramme de câblage*

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

- **Frédéric Metrich** - _Initial work_ - [FredM67](https://github.com/FredM67)

See also the list of [contributors](https://github.com/FredM67/PVRouter-3-phase/graphs/contributors) who participated in this project.
