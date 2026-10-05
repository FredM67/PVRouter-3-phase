# Configurator

A static web page that writes the firmware configuration: `config.h`, `config_system.h` and
`config_rf.h` for the router, `config.h` and `config_rf.h` for each remote unit
(`RemoteLoadReceiver`), and the ESPHome YAML for the mk2Wifi module.

Online: <https://fredm67.github.io/Mk2PVRouter/configurateur/>, published by the documentation
site ([Mk2PVRouter](https://github.com/FredM67/Mk2PVRouter)), from its `3-phase` submodule: the
online configurator always matches the documented firmware (`main`). It also works offline:
open `index.html` in a browser.

No build step, no dependency: plain scripts that also load in Node for the tests.

| File          | Role                                                                     |
| ------------- | ------------------------------------------------------------------------ |
| `model.js`    | the settings, their defaults (= the shipped files) and derived values    |
| `validate.js` | the checks of `validation.h`, plus what the compiler cannot see          |
| `generate.js` | model → the firmware files                                               |
| `yaml.js`     | model → the ESPHome YAML (fail-safe controls, sensors of what is sent)   |
| `i18n.js`     | English and French texts of the page                                     |
| `zip.js`      | "download all"                                                           |
| `folder.js`   | "save into the firmware folder" (File System Access API: Chrome, Edge)   |
| `app.js`      | the page                                                                 |

## Keeping it in sync with the firmware

The default choices must give back the shipped files byte for byte (`@date` aside). After a
change to `Mk2_3phase_RFdatalog_temp/config.h`, `config_system.h`, `config_rf.h` or to the
receiver's config files, update `generate.js` (and `model.js` for a new setting) until the
tests pass again.

```sh
node --test configurator/test/*.test.js           # unit tests
bash configurator/test/compile-presets.sh         # builds every preset (pio, clang-format)
bash configurator/test/check-yaml.sh              # 'esphome config' on the mk2Wifi presets
```

The presets (`test/presets/*.json`) are saves of the page: a new one is picked up by both
scripts. The CI job `Configurator` runs all three.
