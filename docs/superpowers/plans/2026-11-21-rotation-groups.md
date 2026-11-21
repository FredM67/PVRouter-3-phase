# Rotation Groups Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Priority rotation rotates only the loads listed in `rotationGroups`, each group among the priority places its loads hold, instead of every load.

**Architecture:** A settings type `RotationGroups` in `config_types.h` holds the target codes as written in `config.h`. A new Arduino-free header `rotation_groups.h` turns codes into load numbers, checks the groups, and builds `Rotation::Table`: the priority places of each group, resolved at compile time from the load map and `loadPrioritiesAtStartup`. `config_derived.h` builds `rotationTable`; the ISR calls `rotationTable.rotate()` where it called `LoadPriorities::rotate()`. The configurator gets the same setting, editor and checks.

**Tech Stack:** C++17 on avr-gcc 7.3 (ATmega328P), Unity native tests through PlatformIO in WSL, bash compile-fail harness, plain JS configurator with `node --test`.

**Spec:** `docs/superpowers/specs/2026-11-21-rotation-groups-design.md`

## Global Constraints

- No backward compatibility: rotation on with no group does not build; no default group, in the firmware or the configurator.
- Everything resolved at compile time; no RAM growth for a config without rotation (`Rotation::Table` for 0 groups holds no array).
- `config.h` holds settings only (#238): the settings type in `config_types.h`, the resolved object in `config_derived.h`.
- In target lists a plain number is a **pin**; `LOAD(n)`/`LOCAL_LOAD(n)` are load n of `physicalLoadPin`; `REMOTE_LOAD(n)` is the n-th remote load in map order.
- One trigger for every group: `PRIORITY_ROTATION`, `ROTATION_AFTER_SECONDS`, the rotation pin and the off-peak rule are unchanged.
- Rotation direction as today: in each group, the load in the group's last place moves to its first place, the others move one place down; each entry keeps its ON bit.
- Commits end with `Co-Authored-By: Claude <noreply@anthropic.com>`. Never commit `Mk2_3phase_RFdatalog_temp/.vscode/extensions.json` or `Mk2_3phase_RFdatalog_temp/version.h` (builds rewrite it: `git checkout` it back).
- The pre-commit hook rewrites `@date`/`@copyright` and runs clang-format: re-stage and commit again. On "Device or resource busy", retry.
- Native tests only run in WSL: `MSYS_NO_PATHCONV=1 wsl bash -lc 'cd /mnt/d/Git/IoT/pvr-wt/rotation/Mk2_3phase_RFdatalog_temp && ~/.platformio/penv/bin/pio test -e native -f <filter>'`. Node is in WSL too: `~/.local/node/bin/node`.
- French text: U+00A0 before `:` `;` `?` `!`, typographic apostrophe `’`, as the rest of `i18n.js` and `Readme.md`.

## Review Focus

1. A user forgets the inner braces, `rotationGroups{ LOAD(0), LOAD(1) }`: that is two groups of one load each, and the build must stop on "needs at least 2 loads", not build something odd. Pinned by compile-fail case 68 (Task 3).
2. In the configurator, a load is removed from the load map after the groups were set: the stale load number must give the "not a load" error, not a crash or a silently shorter group. Pinned by a validate test (Task 4).
3. `REMOTE_LOAD(n)` and a plain pin inside a group resolve to the right load number when local and remote loads are interleaved in the map. Pinned by `test_loads_of_remote_load_follows_map_order` and `test_loads_of_pin_is_the_local_load_on_it` (Task 2).
4. A group whose loads are not adjacent in priority order (interleaved heaters, or a non-grouped load between them): the loads outside the groups must not move. Pinned by `test_two_interleaved_groups_rotate_apart` and `test_loads_outside_groups_never_move` (Task 2).
5. Rotation with only one local load, or a preset with rotation AUTO and one load (the `sixty-hz-json` preset): no valid group exists, so rotation must be OFF there. Pinned by the preset change and the configurator check (Task 4).

---

### Task 1: `RotationGroups`, the settings type

**Files:**
- Modify: `Mk2_3phase_RFdatalog_temp/config_types.h` (after `ScheduledBoosts` and its guides)
- Modify: `Mk2_3phase_RFdatalog_temp/config.h` (after the `loadPrioritiesAtStartup` line)
- Create: `Mk2_3phase_RFdatalog_temp/test/native/test_rotation_groups/test_main.cpp` and `README.md`
- Modify: `Mk2_3phase_RFdatalog_temp/test/native/test_config_h/test_main.cpp`

**Interfaces:**
- Produces:
  - `template< uint8_t N, uint8_t MaxPins = 16 > class RotationGroups` with `constexpr uint8_t size() const` (= N) and `constexpr const PinList< MaxPins >& getGroup(uint8_t index) const`.
  - Constructor `constexpr RotationGroups(const PinList< MaxPins >& g0, const PinList< MaxPins >& g1 = {}, ... , const PinList< MaxPins >& g9 = {})` storing the first N; `static_assert(N >= 1 && N <= 10)`. Ten groups is more than any valid config can hold (at most 12 local and 8 remote loads, 2 per group).
  - `template< uint8_t MaxPins > class RotationGroups< 0, MaxPins >`: default constructor, `size()` = 0.
  - Deduction guides: `RotationGroups() -> RotationGroups< 0 >;` and one guide per arity 1 to 10, e.g. `RotationGroups(const PinList< 16 >&, const PinList< 16 >&) -> RotationGroups< 2 >;`.
  - In `config.h`: `inline constexpr RotationGroups rotationGroups{}; /**< the loads that take turns: { { LOAD(0), LOAD(1), LOAD(2) }, ... }, {} without rotation */`, preceded by a comment block: groups rotate apart, a load in at most one group, all local or all on one remote unit, at least 2 loads, `{ { ALL_LOCAL_LOADS() } }` for one heater.

- [ ] **Step 1: Write the failing tests** in `test/native/test_rotation_groups/test_main.cpp` (includes `config_types.h` and `load_map.h` only):

```cpp
void test_no_group(void)
{
  constexpr RotationGroups groups{};
  TEST_ASSERT_EQUAL(0, groups.size());
}

void test_one_group_of_every_local_load(void)
{
  constexpr RotationGroups groups{ { ALL_LOCAL_LOADS() } };
  TEST_ASSERT_EQUAL(1, groups.size());
  TEST_ASSERT_EQUAL(1, groups.getGroup(0).count);
  TEST_ASSERT_EQUAL(ALL_LOCAL_LOADS(), groups.getGroup(0).pins[0]);
}

void test_two_groups_keep_their_codes(void)
{
  constexpr RotationGroups groups{ { LOAD(0), LOAD(1), LOAD(2) }, { LOAD(3), LOAD(4), LOAD(5) } };
  TEST_ASSERT_EQUAL(2, groups.size());
  TEST_ASSERT_EQUAL(3, groups.getGroup(1).count);
  TEST_ASSERT_EQUAL(LOAD(4), groups.getGroup(1).pins[1]);
}

void test_missing_inner_braces_make_one_load_groups(void)
{
  constexpr RotationGroups groups{ LOAD(0), LOAD(1) };
  TEST_ASSERT_EQUAL(2, groups.size());
  TEST_ASSERT_EQUAL(1, groups.getGroup(0).count);
}
```

In `test_config_h`, `test_config_h_reads_on_its_own` also asserts `TEST_ASSERT_TRUE(rotationGroups.size() <= NO_OF_DUMPLOADS / 2);`.

- [ ] **Step 2: Run them, they fail**

Run: `... pio test -e native -f native/test_rotation_groups` (WSL command in Global Constraints)
Expected: compile error, `RotationGroups` not declared.

- [ ] **Step 3: Implement `RotationGroups`** in `config_types.h` and the `config.h` line. Storing the first N of ten references: build them into a local array and copy through `make_index_sequence< N >`, as `OverridePins` does.

- [ ] **Step 4: Run the tests, they pass**, and `-f native/test_config_h` too.

Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
git add Mk2_3phase_RFdatalog_temp/config_types.h Mk2_3phase_RFdatalog_temp/config.h Mk2_3phase_RFdatalog_temp/test/native/test_rotation_groups Mk2_3phase_RFdatalog_temp/test/native/test_config_h
git commit -m "feat: RotationGroups, the loads that take turns, in config.h"
```

### Task 2: `rotation_groups.h`, resolution, checks and rotation

**Files:**
- Create: `Mk2_3phase_RFdatalog_temp/rotation_groups.h` (Arduino-free: includes `config_types.h`, `load_map.h`)
- Modify: `Mk2_3phase_RFdatalog_temp/test/native/test_rotation_groups/test_main.cpp` and its `README.md`

**Interfaces:**
- Consumes: `RotationGroups` (Task 1); `Load::isLocal`, `Load::unitOf`, `Load::pinOf` (`load_map.h`); `TargetCode` (`config_types.h`).
- Produces, in `namespace Rotation`:
  - `template< uint8_t NL > constexpr uint32_t loadsOf(uint8_t code, const uint8_t (&loadMap)[NL])`: bit n set for each load n the code stands for; 0 when it stands for no load. `LOAD(n)` → n if n < NL; `LOCAL_LOAD(n)` → n if n < NL and load n is local; `REMOTE_LOAD(n)` → the n-th remote entry of the map; a plain number → the local load on that pin; `ALL_LOCAL_LOADS()`, `ALL_REMOTE_LOADS()`, `ALL_LOADS()` → their loads; relay codes → 0. `static_assert(NL <= 32)`.
  - Checks, each `template< uint8_t NG, uint8_t MaxPins, uint8_t NL > constexpr bool f(const RotationGroups< NG, MaxPins >& groups, const uint8_t (&loadMap)[NL])`, true for 0 groups:
    - `targetsAreLoads`: every code gives a non-zero `loadsOf` (spec check 3);
    - `groupsHaveTwoLoads`: each group's union of loads has at least 2 loads (check 4);
    - `noLoadTwice`: the popcounts of all codes' `loadsOf` add up to the popcount of their union, across every group (check 5);
    - `groupsAreConsistent`: in each group, every load is local, or every load is remote with the same `unitOf` (check 6).
  - `template< uint8_t NG, uint8_t NL > class Table` with constructor `template< uint8_t MaxPins > constexpr Table(const RotationGroups< NG, MaxPins >& groups, const uint8_t (&loadMap)[NL], const uint8_t (&priorities)[NL])` and a deduction guide from it; `constexpr uint8_t size() const` (= NG); `constexpr uint32_t loads(uint8_t g) const` (the group's loads, bit n = load n); `template< uint8_t N > void rotate(uint8_t (&prio)[N]) const`, marked `__attribute__((always_inline)) inline` like `LoadPriorities::rotate`. Each group's places are the positions p where `priorities[p]` is one of its loads, ascending. `Table< 0, NL >` is a specialization with no array and an empty `rotate`.

- [ ] **Step 1: Write the failing tests** (in the same suite; map used by most tests below):

```cpp
// 3 local loads, then 2 on unit 1, then 1 on unit 2
constexpr uint8_t loadMap[]{ Load::local(5), Load::local(6), Load::local(7),
                             Load::remote(1), Load::remote(1), Load::remote(2) };
constexpr uint8_t prio[]{ 0, 1, 2, 3, 4, 5 };

void test_loads_of_load_and_local_load(void)       // LOAD(1) -> 0b10; LOCAL_LOAD(3) -> 0 (remote); LOAD(6) -> 0
void test_loads_of_remote_load_follows_map_order(void)
// with map { local(5), remote(1), local(6), remote(2) }: REMOTE_LOAD(0) -> bit 1, REMOTE_LOAD(1) -> bit 3, REMOTE_LOAD(2) -> 0
void test_loads_of_pin_is_the_local_load_on_it(void) // 6 -> bit 1; 9 -> 0
void test_loads_of_groups_and_relays(void)          // ALL_LOCAL_LOADS() -> 0b000111; ALL_REMOTE_LOADS() -> 0b111000; RELAY(0), ALL_RELAYS() -> 0
void test_checks_pass_for_two_good_groups(void)     // { {LOAD(0),LOAD(1),LOAD(2)}, {LOAD(3),LOAD(4)} }: all four checks true
void test_check_relay_is_not_a_load(void)           // { {LOAD(0), RELAY(0)} }: targetsAreLoads false
void test_check_group_of_one(void)                  // { {LOAD(0)}, {LOAD(1),LOAD(2)} }: groupsHaveTwoLoads false
void test_check_load_twice(void)                    // { {LOAD(0),LOAD(1)}, {LOAD(1),LOAD(2)} } and { {LOAD(0),LOAD(0),LOAD(1)} }: noLoadTwice false
void test_check_mixed_local_and_remote(void)        // { {LOAD(2),LOAD(3)} }: groupsAreConsistent false
void test_check_two_remote_units(void)              // { {LOAD(4),LOAD(5)} }: groupsAreConsistent false
void test_checks_pass_without_groups(void)          // RotationGroups{}: all four true
```

Rotation tests (`ON` = `loadStateOnBit`):

```cpp
void test_one_group_of_every_load_is_todays_rotation(void)
{
  // same result as LoadPriorities::rotate on 0, 1, 2+ON, 3
  constexpr uint8_t map4[]{ Load::local(5), Load::local(6), Load::local(7), Load::local(8) };
  constexpr uint8_t prio4[]{ 0, 1, 2, 3 };
  constexpr Rotation::Table table{ RotationGroups{ { ALL_LOADS() } }, map4, prio4 };
  uint8_t a[]{ 0, 1, 2 | ON, 3 }, b[]{ 0, 1, 2 | ON, 3 };
  table.rotate(a);
  LoadPriorities::rotate(b);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(b, a, 4);
}

void test_two_interleaved_groups_rotate_apart(void)
{
  // priorities 0, 3, 1, 4, 2, 5 with two groups of 3 local loads: places { 0, 2, 4 } and { 1, 3, 5 }
  // one rotation gives 2, 5, 0, 3, 1, 4
}

void test_loads_outside_groups_never_move(void)
{
  // loadMap/prio above, group { LOAD(0), LOAD(1), LOAD(2) }: 0, 1, 2, 3, 4, 5 -> 2, 0, 1, 3, 4, 5
}

void test_on_bits_travel_with_their_load(void)
{
  // group { LOAD(0), LOAD(1) }: { 0 | ON, 1, 2 } -> { 1, 0 | ON, 2 }
}

void test_n_rotations_are_back_to_the_start(void)
{
  // group of 3 rotated 3 times: unchanged
}

void test_table_without_groups_changes_nothing(void)
```

The test file now also includes `rotation_groups.h` and `load_priorities.h`.

- [ ] **Step 2: Run them, they fail**

Expected: compile error, `rotation_groups.h` not found.

- [ ] **Step 3: Implement `rotation_groups.h`.** The per-group rotation, over the group's ascending places `p[0..k)`:

```cpp
const uint8_t last = prio[p[k - 1]];
for (uint8_t i = k - 1; i != 0; --i)
  prio[p[i]] = prio[p[i - 1]];
prio[p[0]] = last;
```

Storage: one flat `uint8_t places_[NL]` (the places of group 0, then group 1...) and `uint8_t ends_[NG]` (the end of each group in `places_`), plus `uint32_t loads_[NG]`.

- [ ] **Step 4: Run the suite, it passes**, then the whole native suite (`pio test -e native`), with `test_load_priorities` unchanged.

Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
git add Mk2_3phase_RFdatalog_temp/rotation_groups.h Mk2_3phase_RFdatalog_temp/test/native/test_rotation_groups
git commit -m "feat: rotation groups resolved to priority places, checked, rotated"
```

### Task 3: The firmware rotates the groups

**Files:**
- Modify: `Mk2_3phase_RFdatalog_temp/config_derived.h`, `processing.cpp` (the `b_reOrderLoads` block), `validation.h`, `utils.h` (after `Load rotation feature`)
- Create: `scripts/ci/compile-fail/60-...sh` to `68-...sh`
- Modify: `sim/configs/relays.patch`

**Interfaces:**
- Consumes: `rotationGroups` (Task 1); `Rotation::Table`, the four checks, `loads(g)` (Task 2).
- Produces: `inline constexpr Rotation::Table rotationTable{ rotationGroups, physicalLoadPin, loadPrioritiesAtStartup };` in `config_derived.h` (include `rotation_groups.h` next to the other `utils_*` includes).

- [ ] **Step 1: Write the compile-fail cases** (format of `12-override-relay-without-relays.sh`; `rotation_on` means `replace 'PRIORITY_ROTATION{ RotationModes::OFF }' 'PRIORITY_ROTATION{ RotationModes::AUTO }'`, written out in each case):

| Case | Change | `expect` |
|---|---|---|
| `60-rotation-without-groups.sh` | rotation AUTO | `Priority rotation is on but rotationGroups is empty` |
| `61-groups-without-rotation.sh` | `rotationGroups{};` → `rotationGroups{ { ALL_LOCAL_LOADS() } };` | `rotationGroups is set but priority rotation is OFF` |
| `62-rotation-relay-target.sh` | AUTO, groups `{ { LOAD(0), RELAY(0) } }` | `A rotation group names something that is not a load` |
| `63-rotation-group-of-one.sh` | AUTO, groups `{ { LOAD(0) } }` | `A rotation group needs at least 2 loads` |
| `64-rotation-load-in-two-groups.sh` | AUTO, groups `{ { LOAD(0), LOAD(1) }, { LOAD(1), LOAD(2) } }` | `A load is listed twice in rotationGroups` |
| `65-rotation-mixed-local-remote.sh` | AUTO, `Load::local(7)` → `Load::remote(1)`, groups `{ { ALL_LOADS() } }` | `A rotation group mixes local and remote loads, or loads of two remote units` |
| `66-rotation-two-remote-units.sh` | AUTO, `Load::local(6)` → `Load::remote(1)`, `Load::local(7)` → `Load::remote(2)`, groups `{ { LOAD(1), LOAD(2) } }` | same as 65 |
| `67-rotation-two-groups-builds.sh` | AUTO, `NO_OF_DUMPLOADS{ 3 }` → `{ 4 }`, map → `Load::local(5), Load::local(6), Load::remote(1), Load::remote(1)`, priorities `{ 0, 1, 2 }` → `{ 0, 1, 2, 3 }`, groups `{ { LOAD(0), LOAD(1) }, { LOAD(2), LOAD(3) } }` | empty (must build) |
| `68-rotation-missing-inner-braces.sh` | AUTO, groups `{ LOAD(0), LOAD(1) }` | `A rotation group needs at least 2 loads` |

Each static_assert message is `"******** <expect text> ! Please check rotationGroups in your config.h ! ********"`.

- [ ] **Step 2: Run them, 60-66 and 68 fail to refuse**

Run: `MSYS_NO_PATHCONV=1 wsl bash -lc 'cd /mnt/d/Git/IoT/pvr-wt/rotation && PIO=~/.platformio/penv/bin/pio scripts/ci/compile-fail.sh scripts/ci/compile-fail/6*.sh'`
Expected: cases 60-66 and 68 reported as built (or failing with another error), 67 fails to build until Step 3.

- [ ] **Step 3: Implement.**
  - `config_derived.h`: `rotationTable`.
  - `processing.cpp`: `rotationTable.rotate(loadPrioritiesAndState);` in place of `LoadPriorities::rotate(loadPrioritiesAndState);`.
  - `validation.h`: six static_asserts with the messages above. Checks 1-2: `(PRIORITY_ROTATION == RotationModes::OFF) || rotationGroups.size() != 0` and `(PRIORITY_ROTATION != RotationModes::OFF) || rotationGroups.size() == 0`; checks 3-6 call the Task 2 checks on `rotationGroups, physicalLoadPin`.
  - `utils.h`: when rotation is on, a line after `Load rotation feature is present`: `\tgroups { 0 1 2 } { 3 4 5 }`, the load numbers of each `rotationTable.loads(g)`, ascending, numbered from 0 as in `LOAD(n)`.
  - `sim/configs/relays.patch` (PIN rotation, 3 local loads): also set `rotationGroups{ { ALL_LOCAL_LOADS() } }`. Regenerate the patch from an edited copy with `diff -u`, then fix its paths to `a/Mk2_3phase_RFdatalog_temp/config.h` / `b/...`.

- [ ] **Step 4: Run every check**
  - `scripts/ci/compile-fail.sh` (all cases): every case passes, the 26 existing ones included.
  - `pio run -e basic` in `Mk2_3phase_RFdatalog_temp`: builds; `avr-size` of `.pio/build/basic/firmware.elf` equals dev's (rotation is OFF in the shipped config, so the firmware must not change).
  - The sim relay scenarios: build the relays firmware as CI does (`sim/configs/sim_config.sh`, `patch -p1 < sim/configs/relays.patch`, `PLATFORMIO_BUILD_DIR=.pio/build-relays pio run -e basic`, restore `config.h`), then `make -C sim check-relays`: all pass.
  - `pio test -e native` and `pio test -e uno_sim`: all pass.

- [ ] **Step 5: Commit**

```bash
git add Mk2_3phase_RFdatalog_temp/config_derived.h Mk2_3phase_RFdatalog_temp/processing.cpp Mk2_3phase_RFdatalog_temp/validation.h Mk2_3phase_RFdatalog_temp/utils.h scripts/ci/compile-fail sim/configs/relays.patch
git commit -m "feat!: priority rotation rotates the rotation groups only"
```

### Task 4: The configurator writes and checks the groups

**Files:**
- Modify: `configurator/model.js`, `validate.js`, `generate.js`, `app.js`, `i18n.js`
- Modify: `configurator/test/presets/remote-units-mk2wifi.json`, `sixty-hz-json.json`
- Modify: `configurator/test/validate.test.js`, `generate.test.js`
- Modify: `Mk2_3phase_RFdatalog_temp/config.h` only if the generator's line differs from Task 1's (the defaults must regenerate it byte for byte)

**Interfaces:**
- Consumes: the `config.h` line of Task 1 (exact text and comment).
- Produces: `m.rotation.groups`, a list of groups, each a list of load numbers (indices into `m.loads`); default `[]`.

- [ ] **Step 1: Write the failing tests**
  - `validate.test.js`, each giving the named error on the `rotation` field:
    - rotation `AUTO`, `groups: []` → `err.rotationNoGroup`;
    - a group `[0]` → `err.rotationGroupSmall`;
    - groups `[[0, 1], [1, 2]]` and `[[0, 0, 1]]` → `err.rotationLoadTwice`;
    - loads local, local, remote unit 1: group `[1, 2]` → `err.rotationGroupMixed`; loads remote unit 1, remote unit 2: group `[0, 1]` → same;
    - a group `[0, 3]` with 3 loads (a load removed after the groups were set) → `err.rotationNotALoad`;
    - rotation `OFF` with groups `[[0, 1]]` → no rotation error;
    - an old save without `groups`, rotation `PIN` → `normalize()` gives `groups: []` and `err.rotationNoGroup`.
  - `generate.test.js`:
    - defaults → `inline constexpr RotationGroups rotationGroups{};` with the Task 1 comment;
    - rotation `AUTO`, groups `[[0, 1, 2]]` with 3 local loads → `rotationGroups{ { ALL_LOCAL_LOADS() } };`;
    - groups `[[0, 1], [2, 3]]` → `rotationGroups{ { LOAD(0), LOAD(1) }, { LOAD(2), LOAD(3) } };`;
    - rotation `OFF` with groups in the model → `rotationGroups{};`.

- [ ] **Step 2: Run them, they fail**

Run: `MSYS_NO_PATHCONV=1 wsl bash -lc 'cd /mnt/d/Git/IoT/pvr-wt/rotation && ~/.local/node/bin/node --test configurator/test/*.test.js'`
Expected: the new tests fail.

- [ ] **Step 3: Implement**
  - `model.js`: `rotation.groups: []` in the defaults.
  - `validate.js`: the five errors above, only when `rotation.mode !== 'OFF'`, in the order not-a-load, too small, twice, mixed; no-group first.
  - `generate.js`: the line right after `loadPrioritiesAtStartup`. `ALL_LOCAL_LOADS()` when a group is exactly every local load (in any order) and there is no other group; otherwise `LOAD(n)` in the group's order.
  - `app.js`: under the rotation select, when the mode is not `OFF`, one sub-block per group: "Group i", a checkbox per load (labelled as the load map labels them), a remove button; then an "add a group" button. A new group starts empty.
  - `i18n.js`, English and French: `rotationGroups` (label), `rotationGroup` ("Group"), `rotationGroupsHelp` (one line: the loads of a group take turns; one group per heater; local loads or the loads of one remote unit), and the five error texts.
  - Presets: `remote-units-mk2wifi.json` gets `"groups": [[0, 1], [2, 3]]` (loads 0-1 local, 2-3 on unit 1, load 4 alone on unit 2); `sixty-hz-json.json` has one load, so its rotation becomes `"mode": "OFF"`.

- [ ] **Step 4: Run every configurator check**
  - `node --test configurator/test/*.test.js`: all pass, the "defaults regenerate the shipped files" test included.
  - `configurator/test/compile-presets.sh` (WSL, with `PIO`): every preset builds.

- [ ] **Step 5: Commit**

```bash
git add configurator Mk2_3phase_RFdatalog_temp/config.h
git commit -m "feat(configurator): rotation groups, written and checked as the compiler does"
```

### Task 5: Docs

**Files:**
- Modify: `Mk2_3phase_RFdatalog_temp/Readme.md` and `Readme.en.md` ("Priority rotation" / "Rotation des priorités")
- Modify: `CHANGELOG.md`
- Modify: `docs/configuration.md` if it lists the settings types of `config_types.h`

- [ ] **Step 1: Readmes.** The rotation section: groups and their rules (at least 2 loads; a load in one group; local loads, or the loads of one remote unit; loads outside groups never move; one trigger for all groups), the one-heater line `rotationGroups{ { ALL_LOCAL_LOADS() } }`, the two-heater example with interleaved priorities `0, 3, 1, 4, 2, 5` → `2, 5, 0, 3, 1, 4`. The paragraph about swapping two devices now reads as a group of those two loads.

- [ ] **Step 2: CHANGELOG.** "Upgrading", a bullet "**Priority rotation needs `rotationGroups` (breaking)**": with rotation on, list the loads that take turns, `{ { ALL_LOCAL_LOADS() } }` for one heater; the build stops otherwise. "Changed": rotation groups, any number, each rotating among its own priority places. Link the PR number once known.

- [ ] **Step 3: Check the links and the French typography** (`cat -A` shows `M-BM- ` before French colons), then commit.

```bash
git add Mk2_3phase_RFdatalog_temp/Readme.md Mk2_3phase_RFdatalog_temp/Readme.en.md CHANGELOG.md docs/configuration.md
git commit -m "docs: rotation groups"
```
