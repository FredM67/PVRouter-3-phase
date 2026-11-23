# Enregistreur de TC (CT recorder)

[![Français](https://img.shields.io/badge/🇫🇷%20Langue-Français-blue?style=for-the-badge)](README.md) [![English](https://img.shields.io/badge/🌍%20Language-English-red?style=for-the-badge)](README.en.md)

---

Un croquis autonome qui enregistre la chaîne ADC du routeur (une tension et un courant, sans calibration), et un outil Python (`analysis/ct_recorder/`) qui cherche le filtre du TC (`lpf_gain` et `alpha` dans `calibration.h`) pour votre carte. Il peut aussi conclure qu'aucun filtre n'est nécessaire.

> **Pas encore testé sur une vraie carte.** Le croquis et l'outil sont vérifiés par les tests natifs, par le simulateur et par des enregistrements synthétiques, pas sur un routeur réel.

## Câblage

- Seule la charge de la paire enregistrée est branchée et pilotée. Les autres charges sont éteintes.
- La charge est pilotée par la broche 5 (la première charge des deux configurations livrées), à travers la sortie triac ou SSR à passage par zéro du routeur, pas un relais. Les broches 6 et 7 restent à l'état OFF.
- Une charge résistive (chauffe-eau) sert de référence.
- La paire enregistrée est V1/I1 en triphasé, V/CT1 en monophasé (`CTR_PAIR=GRID`). Pour V/CT2, mettez `CTR_PAIR=DIVERTED` dans `platformio.ini`.
- Le TC enregistré ne doit voir que le courant de la charge. I1 et CT1 sont les TC du réseau : ils voient toute la maison et l'export du PV. Placez le TC autour du seul câble d'alimentation du chauffe-eau. Sinon, enregistrez avec tout le reste éteint et sans PV (la nuit, par exemple). CT2 (`DIVERTED`) ne voit normalement que la charge routée.

## Choisir l'environnement

Un environnement par type de routeur et de carte (dossier `dev/CtRecorder`) :

| Environnement | Routeur | Carte | Écart entre échantillons |
|---|---|---|---|
| `three_phase_old` | triphasé | ancienne | 624 µs |
| `three_phase_new` | triphasé | nouvelle | 624 µs |
| `single_phase_old_seg` | monophasé | ancienne, afficheur 7 segments | 312 µs |
| `single_phase_old_oled` | monophasé | ancienne, OLED | 312 µs |
| `single_phase_new` | monophasé | nouvelle | 312 µs |

Il faut un jeu de valeurs par type de TC et par écart entre échantillons.

## Marche à suivre

Depuis la racine du dépôt. Installation de l'outil (une fois) :

```bash
pip install -r analysis/ct_recorder/requirements.txt
```

1. **Téléverser le croquis** (remplacez l'environnement) :

   ```bash
   cd dev/CtRecorder
   pio run -e three_phase_old -t upload
   cd ../..
   ```

2. **Enregistrer sans filtre**, au moins 3 minutes (180 s) avec le motif d'impulsions par défaut :

   ```bash
   cd analysis
   python -m ct_recorder.capture COM3 --seconds 180 -o sans_filtre.ctr
   ```

   `COM3` est le port série (500 000 bauds). Ouvrir le port peut réinitialiser la carte. Le croquis refuse alors d'enregistrer pendant 120 s, et la capture attend. Comptez donc environ 2 minutes de plus par capture. Avec `--no-wait`, la capture s'arrête avec une erreur au lieu d'attendre. Le motif se change avec `--pattern ON,OFF[,ON,OFF...]` (en cycles secteur, 8 paires au plus).

3. **Analyser** :

   ```bash
   python -m ct_recorder.analyse sans_filtre.ctr --plots graphiques
   ```

   Le rapport donne le filtre trouvé et le verdict. Avec `--plots`, `switch_on.png` et `switch_off.png` montrent le courant brut, filtré et la référence pour la plus longue impulsion.

4. **Vérifier avec le filtre** (valeurs données par le verdict), puis comparer :

   ```bash
   python -m ct_recorder.capture COM3 --seconds 180 --filter 0.004626 9.344 -o avec_filtre.ctr
   python -m ct_recorder.analyse sans_filtre.ctr --compare avec_filtre.ctr
   ```

   `0.004626` et `9.344` sont des valeurs d'exemple, tirées d'un enregistrement synthétique. Utilisez celles qu'affiche votre analyse. `--filter ALPHA GAIN` est accepté pour alpha de 0,00005 à moins de 0,0625 et un gain de 0 à moins de 64. Un gain de 0 coupe le filtre (c'est l'état à la mise sous tension).

## Règles à respecter

- Le croquis refuse d'enregistrer (`#ERR warming up`) pendant les 120 premières secondes après la mise sous tension ou une réinitialisation. L'ouverture du port série peut réinitialiser la carte (selon la carte et le pilote). Le filtre de décalage continu du firmware se stabilise avec une constante de temps d'environ 20 s en triphasé (environ 10 s en monophasé). La capture attend et réessaie, sauf avec `--no-wait`.
- Enregistrez au moins 3 minutes avec le motif par défaut (au moins 10 impulsions environ). Avec moins, l'estimation de la constante de temps du TC n'est pas fiable.
- Les commandes ne sont pas lues pendant un enregistrement (tampon d'entrée série de 64 octets).

## Lire le résultat

L'analyse mesure l'erreur d'énergie sur les cycles ON de chaque impulsion. Pour un TC linéaire, la queue à la coupure reflète l'affaissement à l'enclenchement : sur une impulsion et sa queue, l'erreur s'annule (colonne « with tail », proche de 0). Le compteur d'énergie du routeur intègre tous les cycles, donc l'affaissement du TC ne fausse pas l'énergie à long terme. Le filtre remet l'énergie dans les bons cycles, ce qui compte pour les décisions du routeur cycle par cycle.

- **« no filter needed »** : l'amélioration est dans la dispersion d'une impulsion à l'autre. Laissez `lpf_gain` à 0.
- **« filter needed »** : le verdict imprime les deux lignes à coller dans `Mk2_3phase_RFdatalog_temp/calibration.h` :

  ```cpp
  inline constexpr float lpf_gain{ 9.344 }; /**< setting this to 0 disables this extra processing */
  inline constexpr float alpha{ 0.004626 };
  ```

  Ces valeurs sont des exemples, tirés d'un enregistrement synthétique. Utilisez celles qu'affiche votre analyse.

- Le décalage V/I ajusté est limité à ±2 ms (36° à 50 Hz), avec un avertissement à la limite.
- Si le courant n'est pas proportionnel à la tension (charge non résistive, ou TC sur une autre phase), l'analyse refuse et ne donne pas de filtre.
- Une note indique quand la fin de la queue du TC ne peut pas être confirmée avec 10 bits (queue trop courte devant la résolution).
- Les impulsions des 10 premières secondes, avec un dépassement du tampon, ou sans courant (thermostat du chauffe-eau coupé), sont écartées et listées. S'il reste moins de 4 impulsions, l'analyse refuse.

## Fichiers

- `main.cpp` : le croquis (commandes `F` filtre, `B` motif, `R` durée, lignes d'en-tête, `#BEGIN`/`#END`).
- `sample_path.h`, `chain_layout.h`, `burst_pattern.h`, `frame.h`, `ring_buffer.h` : la chaîne d'échantillons comme le firmware, la disposition des voies, le motif, le format des trames, le tampon.
- `test/` : tests natifs (`pio test -e native`).
- Le firmware n'est pas modifié : le croquis inclut `ct_filter.h` et `mult_asm.h` de `Mk2_3phase_RFdatalog_temp/`.
