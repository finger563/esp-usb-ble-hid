# Documentation

Guides for using, configuring, updating and building the ESP USB BLE HID
dongle. New here? Start with [Getting started](getting-started.md), or go back
to the [project README](../README.md).

## Use

| Page | What's there |
|---|---|
| [Getting started](getting-started.md) | What you need, install the firmware, pair a controller, next steps for your host |
| [Hosts](hosts.md) | Nintendo Switch, PC / Steam Deck / Android in Xbox 360 mode, macOS and iOS, Linux permissions |
| [Controllers](controllers.md) | Which controllers work, how to tell Bluetooth LE from Classic, pairing, the default layout, what is forwarded |
| [Button mapping](button-mapping.md) | Profiles per controller, the four kinds of rules, presets, Learn, recipes |
| [Troubleshooting and FAQ](troubleshooting.md) | Fixes by symptom, frequently asked questions, what to include in a bug report |

## Maintain

| Page | What's there |
|---|---|
| [Web console](web-console.md) | Opening it, auto-connect, every tab and setting |
| [Updating](updating.md) | One-click updates, installing your own build, confirm or roll back, the one-time Full update, command-line updates |

## Build

| Page | What's there |
|---|---|
| [Hardware](hardware.md) | Supported boards, LED and button, the status screen, low-power idle, download mode |
| [Building](building.md) | Toolchain, building for each board, configuration options, the first flash and the dev loop, tests |
| [How it works](architecture.md) | Data flow, source map, the console's modules, the espp components it is built on, adding a module |

The web console's protocol-level notes (frames, modules, its tests and the
vendored esptool-js) are in [`web/README.md`](../web/README.md).
