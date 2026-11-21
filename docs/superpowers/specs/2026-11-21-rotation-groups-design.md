# Rotation groups: say which loads take turns

Status: design, awaiting approval.

## Intent

Priority rotation lets the elements of a 3-phase water heater take turns at the top of the priority list, so that they wear evenly and the tank heats evenly. Today it rotates **every** load: `LoadPriorities::rotate()` moves the last load in priority order to the front and every other load down one place. Nothing says which loads rotate.

As soon as the load map holds anything besides one heater, that is wrong. With a 3-phase heater on loads 0-2 (local) and a towel radiator on a remote unit (load 3), the priorities `0, 1, 2, 3` become `3, 0, 1, 2`: the radiator jumps to the top. What the user wants is `2, 0, 1, 3`.

A customer once had two 3-phase water heaters and wanted each one's elements to rotate on their own: two groups of three loads. Rotating a mix of local and remote loads, or loads of two remote units, makes no sense: they are different appliances.

## Goals

- Say in `config.h` which loads rotate, as **groups**: each group rotates among itself, and loads in no group never move.
- Any number of groups. No explicit limit: in practice one or two.
- The same target syntax as `overridePins` and `scheduledBoosts`: `LOAD(n)`, `LOCAL_LOAD(n)`, `REMOTE_LOAD(n)`, `ALL_LOCAL_LOADS()`...
- Wrong groups are rejected at compile time with a message naming the problem, as `validation.h` does.
- The configurator writes the new setting and checks the same rules.

## Non-goals

- A trigger per group. `PRIORITY_ROTATION` (`AUTO`, `PIN`, and the start of off-peak with a dual tariff) stays the single trigger: it rotates every group at once.
- Changing what triggers a rotation, `ROTATION_AFTER_SECONDS`, the rotation pin or the off-peak rule.
- Rotating relays. Relays have no priority place; they follow their own thresholds.
- Changing the priority list itself (`loadPrioritiesAtStartup`) or how loads are numbered.

## Anti-goals

- **No backward compatibility.** A config with rotation on and no groups does not build. No default group, no "rotate everything when nothing is said".
- No RAM growth for a config without rotation, and no measurable cost in the ISR: the rotation runs once a day at most, but in the ISR.
- No new syntax for the user to learn: the groups are written like the override and boost targets.

## Constraints

- ATmega328P, avr-gcc 7.3, C++17, LTO. Everything resolved at compile time.
- `config.h` holds settings only (#238): the settings type lives in `config_types.h`, the resolved table in `config_derived.h`, as `OverridePins` → `OverrideTable` and `ScheduledBoosts` → `BoostSchedule`.
- `loadPrioritiesAtStartup` lists load numbers in priority order: place 0 holds the top-priority load (`processing.cpp`, `initializeProcessing`). `loadPrioritiesAndState` is that list, with each load's ON bit; the ISR rotates it when `Shared::b_reOrderLoads` is set (`processing.cpp`), and the bit travels with its load.
- The load map records, at compile time, whether each load is local and its remote unit (`Load::isLocal`, `Load::unitOf` in `load_map.h`).
- In the target lists, a plain number is a **pin** (`overridePins`). `LOAD(n)` and `LOCAL_LOAD(n)` are load n of `physicalLoadPin`; `REMOTE_LOAD(n)` is the n-th remote load in map order.

## Approaches

**A. A list of groups, written with the target syntax (chosen).**

```cpp
inline constexpr RotationGroups rotationGroups{
  { LOAD(0), LOAD(1), LOAD(2) },  // heater 1
  { LOAD(3), LOAD(4), LOAD(5) }   // heater 2
};
```

It reads like the rest of `config.h`, `{ ALL_LOCAL_LOADS() }` covers the common single-heater case, and it follows the settings → resolved-table pattern of #238. The user: "Simple, clear and match other features."

**B. One group number per load**, next to the load map (`rotationGroupOf[NO_OF_DUMPLOADS]{ 1, 1, 1, 2, 2, 2 }`, 0 = never). Simpler to check, but a bare column of numbers to keep aligned with `physicalLoadPin`, unlike the other target lists. Not chosen.

**C. "The first K loads in priority order rotate"** (`ROTATED_LOADS{ 3 }`). One group only, and it forces the rotated loads to be the top priorities. Ruled out by the two-heater case.

## Design

### The setting

In `config.h`, next to `loadPrioritiesAtStartup`:

```cpp
inline constexpr RotationGroups rotationGroups{};  // no rotation
inline constexpr RotationGroups rotationGroups{ { ALL_LOCAL_LOADS() } };  // one heater
inline constexpr RotationGroups rotationGroups{ { LOAD(0), LOAD(1), LOAD(2) }, { LOAD(3), LOAD(4), LOAD(5) } };
```

- `RotationGroups` is a settings type in `config_types.h`: N groups, each a target list (codes, as in `OverridePins`), with deduction guides, and an empty form for no group. It holds the codes only.
- Allowed in a group: `LOAD(n)`, `LOCAL_LOAD(n)`, `REMOTE_LOAD(n)`, `ALL_LOCAL_LOADS()`, `ALL_REMOTE_LOADS()`, `ALL_LOADS()`, and a plain number, which is the pin of a local load. Relay codes (`RELAY(n)`, `ALL_RELAYS()`, `ALL_LOADS_AND_RELAYS()`) are refused.
- A group code expands to its loads; the rules below then apply to the expanded group. `ALL_REMOTE_LOADS()` and `ALL_LOADS()` pass only when their loads satisfy them (one remote unit; all local or all on one unit).

### Resolution (`config_derived.h`)

A resolved object, built from `rotationGroups`, `physicalLoadPin` and `loadPrioritiesAtStartup`, gives for each group the **priority places** its loads hold at startup, in ascending order. Rotation only reorders loads within those places, so the places of a group never change: they are compile-time constants.

The code → load-number step is new (`TargetMap` turns codes into pins): `LOAD(n)`/`LOCAL_LOAD(n)` → n, `REMOTE_LOAD(n)` → the load of the n-th remote entry, a pin → the local load on that pin, a group code → its loads.

### Rotation

When `Shared::b_reOrderLoads` is set, the ISR rotates each group by one among its places, in the same direction as today: the load in the group's last place moves to its first place, the others move one place down. Each value moves with its ON bit, as today. Loads in no group never move.

A single group holding every load gives exactly today's rotation: this is the regression check for the new code.

Example, priorities `0, 3, 1, 4, 2, 5` (two heaters interleaved), groups `{0, 1, 2}` and `{3, 4, 5}`: the places are `{0, 2, 4}` and `{1, 3, 5}`, and one rotation gives `2, 5, 0, 3, 1, 4`.

### Checks (`validation.h`, compile time)

Each with a message naming `rotationGroups` and the problem:

1. Rotation on (`PRIORITY_ROTATION` not `OFF`) and no group.
2. Groups given with rotation `OFF`.
3. A target that is not a load: a relay code, `LOAD(n)` beyond the loads, `LOCAL_LOAD(n)` on a remote load, `REMOTE_LOAD(n)` beyond the remote loads, a pin with no local load on it.
4. A group of fewer than 2 loads, after expansion.
5. A load in two groups, or twice in one group.
6. A group mixing local and remote loads, or loads of two remote units.

A config of the former layout (rotation on, no `rotationGroups` declared) fails on the undeclared name. If a static_assert with a clear message can be had for that case, take it; otherwise the compiler's "`rotationGroups` was not declared" and the CHANGELOG entry are enough.

### Startup summary

Under the line that says rotation is present, a tab-indented line lists the groups as load numbers, as the other features print their details, e.g. `Load rotation feature is present` then `\tgroups { 0 1 2 } { 3 4 5 }`. `logLoadPriorities()` is unchanged.

### Configurator

- The model gets `rotation.groups`: a list of groups, each a list of load numbers. Default: no group.
- The rotation section gets a group editor: add or remove a group, tick its loads, as the boost targets are edited.
- The generator writes `rotationGroups` next to `loadPrioritiesAtStartup`, `{}` when rotation is `OFF`; `LOAD(n)` entries, or `ALL_LOCAL_LOADS()` when a group is exactly every local load.
- Its checks mirror the compiler's 1 and 3-6, in English and French. Check 2 cannot arise there: with rotation `OFF` the group editor is hidden and the generator writes `{}`, whatever groups the model still holds.
- An old save file with rotation on loads with no group and fails check 1, as the firmware does: no default group is added (anti-goal).
- The default choices still regenerate the shipped `config.h` byte for byte: the shipped file gains `inline constexpr RotationGroups rotationGroups{};`.

### Tests

- Native: the group rotation (one group of all loads = today's rotation; two interleaved groups; loads outside groups never move; ON bits travel), and the code → load-number resolution.
- Compile-fail cases (`scripts/ci/compile-fail/`): one per check 1-6, plus a two-group config that builds.
- Configurator tests: the new checks and the generated line.
- Configs with rotation on get their groups, chosen by hand: the configurator presets `remote-units-mk2wifi.json` (PIN; local loads and two remote units, so one group per kind, e.g. `{0, 1}` and `{2, 3}` as its map allows) and `sixty-hz-json.json` (AUTO), and `sim/configs/relays.patch` (PIN).

### Docs

- The "Priority rotation" section of both firmware Readmes: groups, the rules, the two-heater example.
- CHANGELOG, "Upgrading": rotation now needs `rotationGroups`; with one heater, `{ { ALL_LOCAL_LOADS() } }` or the listed loads. "Changed": rotation groups.

## Calls made (not asked)

- The `AUTO` trigger is unchanged: it counts the time the load in the top priority place stays OFF (`processing.cpp`, `absenceOfDivertedEnergyCountInMC`). Loads switch ON in priority order, so that load being OFF means nothing is diverted, whether it is in a group or not.
- `LOCAL_LOAD(n)` on a remote load is "not a load" (check 3).
- Old configurator save files with rotation on fail check 1; the configurator does not invent a group.

## Left to the builder

- The exact type and member names, the storage of the resolved places (one flat array plus group bounds, or per-group arrays), and how the ISR loops over them.
- Whether the config line for one heater is `{ { ALL_LOCAL_LOADS() } }` or `{ ALL_LOCAL_LOADS() }`: whichever the type allows without ambiguity, used the same in the Readme, the configurator and the CHANGELOG.
- The wording of the error messages.
