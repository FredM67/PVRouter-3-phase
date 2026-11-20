# Guide de Réglage du Filtre EWMA pour l'Immunité aux Nuages

[![Français](https://img.shields.io/badge/🇫🇷%20Langue-Français-blue?style=for-the-badge)](EWMA_TUNING_GUIDE.md) [![English](https://img.shields.io/badge/🌍%20Language-English-red?style=for-the-badge)](EWMA_TUNING_GUIDE.en.md)

---

## Vue d'Ensemble

Le filtre EWMA (Exponentially Weighted Moving Average) dans le Routeur PV empêche les oscillations des relais causées par les nuages passant au-dessus des panneaux solaires. Ce guide explique comment régler le filtre pour des performances optimales.

## Améliorations Récentes

### 1. Algorithme de Filtre Amélioré
- **Changement d'EMA vers TEMA** : Le contrôle des relais utilise maintenant le Triple EMA (`getAverageT()`) au lieu de l'EMA simple
- **Meilleure Immunité aux Nuages** : TEMA fournit une immunité supérieure aux ombres nuageuses brèves tout en maintenant une bonne réactivité aux changements réels
- **Performances Optimales** : TEMA offre le meilleur équilibre entre stabilité et réactivité

### 2. Compréhension des Systèmes à Batterie 🔋 **NOUVEAU**
- **Solution de Configuration** : Configuration appropriée du seuil d'importation pour les systèmes à batterie
- **Approche Basée sur la Physique** : Prendre en compte les fluctuations de mesure autour de zéro
- **Simple et Robuste** : Pas de contournements complexes nécessaires, juste une configuration appropriée

### 3. Configuration Facile
De nouveaux paramètres de configuration ont été ajoutés à `config.h` :

```cpp
inline constexpr uint8_t RELAY_FILTER_DELAY_MINUTES{ 2 };  /**< Délai du filtre EWMA en minutes */
```

Pour les systèmes à batterie, voir `../../Mk2_3phase_RFdatalog_temp/BATTERY_CONFIGURATION_GUIDE.md` pour la configuration appropriée des seuils.

## Directives de Réglage

### Constantes de Temps du Filtre

| Type de Région | Délai Recommandé | Caractéristiques |
|----------------|------------------|------------------|
| **Ciel Dégagé** | 1 minute | Réponse rapide, nuages minimaux |
| **Conditions Mixtes** | 2 minutes | Performance équilibrée (par défaut) |
| **Très Nuageux** | 3+ minutes | Stabilité maximale, nuages fréquents |

### Comment Régler

1. **Éditer `config.h`** : Changer la valeur de `RELAY_FILTER_DELAY_MINUTES`
2. **Recompiler et téléverser** le firmware
3. **Surveiller les performances** pendant quelques jours
4. **Ajuster si nécessaire** :
   - Si les relais oscillent durant les nuages → Augmenter le délai
   - Si la réponse est trop lente → Diminuer le délai

### Détails Techniques

#### Implémentation Actuelle
- **Taux d'Échantillonnage** : 5 secondes (configurable via `DATALOG_PERIOD_IN_SECONDS`)
- **Type de Filtre** : Triple EMA (TEMA) pour une immunité optimale aux nuages
- **Délai par Défaut** : 2 minutes (24 échantillons × 5 secondes)

#### Calcul des Valeurs Alpha
Pour un délai de N minutes :
```cpp
samples = (N * 60) / DATALOG_PERIOD_IN_SECONDS
A = samples  // Paramètre de lissage
alpha_ema = 1/16        // Base stable
alpha_ema_ema = 1/8     // 2x plus rapide
alpha_ema_ema_ema = 1/4 // 4x plus rapide
```

## Indicateurs de Performance

### Bon Réglage
- Les relais commutent en douceur lors de vrais changements de puissance
- Pas d'oscillations durant les ombres nuageuses brèves
- Le système répond de manière appropriée aux changements de charge

### Besoin d'Augmenter le Délai
- Les relais s'allument/éteignent rapidement durant les nuages
- Le système semble "nerveux" ou instable
- Sons fréquents de commutation des contacts de relais

### Besoin de Diminuer le Délai
- Le système répond lentement aux changements de charge
- Prend trop de temps pour commencer la diversion après une augmentation solaire
- Réponse lente aux changements de consommation

## Configuration Avancée

Pour les utilisateurs qui veulent expérimenter avec différents types de filtres, le système de relais peut être modifié pour utiliser :

- **EMA** : `ewma_average.getAverageS()` - Plus réactif, moins de filtrage
- **DEMA** : `ewma_average.getAverageD()` - Bon équilibre
- **TEMA** : `ewma_average.getAverageT()` - Meilleure immunité aux nuages (par défaut actuel)

La sélection du filtre se trouve dans `relay_engine.h` dans les méthodes `get_average()` et `proceed_relays()`.

### Comparaison Visuelle des Filtres

Pour une analyse détaillée des performances des filtres, voir l'analyse complète dans `TEMA_ANALYSIS_README.md` qui inclut :

![Comparaison des Filtres](../plots/ema_dema_tema_comparison_extended.png)

Cette analyse démontre pourquoi TEMA est le choix optimal pour les applications de routeur PV.

## Dépannage

### Problèmes Courants

1. **Oscillations de relais persistantes** : Augmenter `RELAY_FILTER_DELAY_MINUTES`
2. **Réponse trop lente** : Diminuer `RELAY_FILTER_DELAY_MINUTES`
3. **Comportement incohérent** : Vérifier que `DATALOG_PERIOD_IN_SECONDS` est approprié pour votre installation

### Débogage Avancé

Surveiller les valeurs du filtre EWMA à travers la sortie série ou la télémétrie pour comprendre le comportement du filtre :
- Valeurs de puissance brutes vs valeurs filtrées
- Temps de réponse du filtre aux changements d'échelon
- Stabilité durant les événements nuageux

---

*Ce guide de réglage aide à optimiser votre Routeur PV pour vos conditions météorologiques spécifiques et les exigences d'installation.*
