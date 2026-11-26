# Guide de Réglage du Filtre EWMA pour l'Immunité aux Nuages

[![Français](https://img.shields.io/badge/🇫🇷%20Langue-Français-blue?style=for-the-badge)](EWMA_TUNING_GUIDE.md) [![English](https://img.shields.io/badge/🌍%20Language-English-red?style=for-the-badge)](EWMA_TUNING_GUIDE.en.md)

---

## Vue d'ensemble

Les relais ne commutent pas sur la puissance instantanée, mais sur une moyenne de celle-ci, pour qu'un nuage passant au-dessus des panneaux ne les fasse pas osciller. Cette moyenne est une moyenne mobile exponentielle triple (TEMA, `getAverageT()` dans `ewma_avg.hpp`), utilisée par `relay_engine.h`.

## Le réglage

La durée de la moyenne, en minutes, est `RELAY_FILTER_DELAY` dans `config.h`, passée aux relais avec `MINUTES()`. Tel que fourni :

```cpp
inline constexpr uint8_t RELAY_FILTER_DELAY{ 2 }; /**< EWMA filter delay in minutes for relay control */

inline constexpr RelayEngine relays{ MINUTES(RELAY_FILTER_DELAY),
                                     { { unused_pin, 100, 200, 1, 1 },
                                       { unused_pin, 300, 400, 1, 1 },
                                       { unused_pin, 500, 600, 1, 1 } } };
```

Le guide rapide de `config.h` :

| Type de région | `RELAY_FILTER_DELAY` |
|----------------|----------------------|
| Ciel dégagé (désert, climat sec) | 1 minute : réponse rapide |
| Conditions mixtes (la plupart des lieux) | 2 minutes : valeur par défaut recommandée |
| Souvent nuageux (côtes) | 3 minutes : plus de stabilité |
| Très nuageux (montagne, tropiques) | 4 minutes : stabilité maximale |

La compilation vérifie que la moyenne couvre au moins 5 périodes de datalog
(`RELAY_FILTER_DELAY * 60 / DATALOG_PERIOD_IN_SECONDS >= 5`, dans `validation.h`) et, avec des relais,
que `DATALOG_PERIOD_IN_SECONDS` divise 60 : 1 minute demande une période de datalog de 12 s ou moins.

## Comment régler

1. Modifier `RELAY_FILTER_DELAY` dans `config.h`.
2. Compiler et téléverser le firmware.
3. Observer les relais pendant quelques jours :
   - les relais s'allument et s'éteignent au passage des nuages : l'augmenter ;
   - la diversion reprend trop tard quand le soleil revient, ou la réponse à la consommation est lente : le diminuer.

Avec la diversion par relais, le datalog affiche la moyenne utilisée par les relais : `R` dans les sorties IoT et JSON, après la puissance totale (`P:<puissance>/<moyenne>`) dans la sortie lisible.

## Détails techniques

- Une valeur par période de datalog (`DATALOG_PERIOD_IN_SECONDS`, 5 s par défaut).
- Le paramètre template de la moyenne est `RELAY_FILTER_DELAY * 60 / DATALOG_PERIOD_IN_SECONDS`
  (`relay_engine.h`) ; ses facteurs de lissage sont arrondis à des puissances de deux, pour ne calculer
  qu'avec des décalages (`ewma_avg.hpp`).

## Systèmes à batterie

Avec une batterie, le seuil d'import d'un relais est négatif : le relais s'éteint quand le surplus
descend sous sa valeur absolue. D'après les commentaires de `config.h` :

```cpp
//   Battery installation: { pin, 1000, -50, 5, 5 }   // Turn OFF when surplus < 50W
```

Voir le [guide de configuration batterie](../../Mk2_3phase_RFdatalog_temp/docs/BATTERY_CONFIGURATION_GUIDE.md).

## Autres filtres

`ewma_avg.hpp` fournit aussi une EMA simple (`getAverageS()`, la plus réactive) et une EMA double
(`getAverageD()`). Les relais utilisent la TEMA ; l'appel se trouve dans `get_average()` et
`proceed_relays()` de `relay_engine.h`.

Pour une comparaison des trois filtres, voir l'[analyse TEMA](TEMA_ANALYSIS_README.md) :

![Comparaison des Filtres](../plots/ema_dema_tema_comparison_extended.png)
