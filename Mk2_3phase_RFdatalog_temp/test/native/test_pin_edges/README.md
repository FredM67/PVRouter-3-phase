# Input Pin Edge Tests

Tests for the edges of the active-LOW input pins (`pin_edges.h`): the diversion, router OFF, dual tariff and rotation pins, read once per second. Each pin keeps its previous level, released at startup, in a static of its own `trackPinEdges<pin>()`; `main.cpp` only reads the pin (`readPinEdges<pin>()`). The static lives for the whole run, so each test uses pins of its own.

## Tests

| Test | Description |
|------|-------------|
| `test_pinEdges_from_the_two_levels` | The four cases of previous and current level: no change, press, held, release |
| `test_changed_is_either_edge_active_is_the_level` | `changed` on both edges, `active` the level, edge or not |
| `test_startup_is_released` / `test_low_at_startup_is_a_press` | The previous level is released at startup: HIGH first is no edge, LOW first is a press |
| `test_press_once_then_held_is_no_edge` | A press gives `pressed()` once, then no edge while held |
| `test_release_once_then_no_edge` | A release gives `released()` once, then no edge |
| `test_each_pin_keeps_its_own_level` | The edges of one pin leave the other pins alone |
| `test_press_release_press` | A sequence of presses and releases, one edge each |
| `test_rotation_on_a_press_when_nothing_is_forced` / `test_no_rotation_while_forced` | `isRotationRequested()`: a press with nothing forced, only |
| `test_a_press_while_forced_is_dropped_not_carried_over` | The rotation pin is read every second, forced or not: pressed during forcing and held past its end, no rotation; a new press afterwards rotates |

## Running

```bash
pio test -e native -f "*test_pin_edges*"
```
