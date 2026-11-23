<h1 align="center">ESP USB BLE HID</h1>

<p align="center">
  <strong>Play with almost any Bluetooth LE gamepad on a Nintendo Switch or a PC, through a tiny USB dongle.</strong><br>
  Bluetooth LE in, a wired Switch Pro or Xbox 360 controller out, with your button layout stored on the dongle.<br>
  No drivers, no app, no soldering.
</p>

<p align="center">
  <a href="https://github.com/finger563/esp-usb-ble-hid/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/finger563/esp-usb-ble-hid?label=release"></a>
  <a href="https://github.com/finger563/esp-usb-ble-hid/actions/workflows/build.yml"><img alt="Build" src="https://github.com/finger563/esp-usb-ble-hid/actions/workflows/build.yml/badge.svg"></a>
  <a href="https://github.com/finger563/esp-usb-ble-hid/actions/workflows/static_analysis.yml"><img alt="Static analysis" src="https://github.com/finger563/esp-usb-ble-hid/actions/workflows/static_analysis.yml/badge.svg"></a>
  <a href="https://finger563.github.io/esp-usb-ble-hid/dongle_console.html"><img alt="Web console" src="https://img.shields.io/badge/web%20console-open%20in%20browser-1fb6c9"></a>
  <a href="LICENSE"><img alt="MIT license" src="https://img.shields.io/badge/license-MIT-blue"></a>
</p>

<p align="center">
  <a href="docs/getting-started.md">Get started</a> ·
  <a href="docs/controllers.md#compatibility">Will my controller work?</a> ·
  <a href="docs/button-mapping.md">Button mapping</a> ·
  <a href="https://finger563.github.io/esp-usb-ble-hid/dongle_console.html">Web console</a> ·
  <a href="docs/troubleshooting.md">Troubleshooting</a> ·
  <a href="docs/README.md">All docs</a>
</p>

<p align="center">
  <img src="https://github.com/user-attachments/assets/d76558db-c34e-48d4-9771-06fa4ebdb05a" alt="A LilyGo T-Dongle-S3 running the firmware, plugged into a Switch dock, next to an Xbox controller" width="720">
</p>

The dongle is a small ESP32-S3 board running this firmware. It pairs with your
controller over Bluetooth LE and plugs into the console or computer as a
**wired** controller: a Switch Pro Controller for the Switch, or an Xbox 360
controller for Windows, Linux, Steam / Steam Deck and Android. The host never
knows a wireless controller is involved. Remaps, dead zones and per-controller
profiles live on the dongle, so they work in every game and on every host you
plug it into. A web page in your browser configures and updates the dongle over
the same USB plug.

## Where to start

| I want to… | Go to |
|---|---|
| Play on my Switch with a Bluetooth controller | [Quick start](#quick-start) |
| Use a Bluetooth pad as an Xbox 360 controller on a PC, Steam Deck or Android | [Quick start](#quick-start), then [Xbox 360 mode](docs/hosts.md#switch-to-xbox-360-mode) |
| Check that my controller works | [Controller compatibility](docs/controllers.md#compatibility) |
| Change the button layout (Nintendo A/B, one-handed play, d-pad on a stick…) | [Button mapping guide](docs/button-mapping.md) |
| Update my dongle, or move from v3 to v4 | [Updating the firmware](docs/updating.md) |
| Build it for another ESP32-S3 board, or hack on it | [Hardware](docs/hardware.md), [Building](docs/building.md), [How it works](docs/architecture.md) |
| Fix something that isn't working | [Troubleshooting and FAQ](docs/troubleshooting.md) |

## What you get

**Play**
- **Almost any Bluetooth LE gamepad.** The dongle reads the button layout each controller publishes, so it isn't tied to one model. Xbox Wireless Controllers and the Bluetooth LE modes of many other pads work. [Compatibility →](docs/controllers.md#compatibility)
- **Two controllers in one.** A wired **Switch Pro Controller** for the Switch, or a wired **Xbox 360 controller** (X-Input, with analog triggers and the Guide button) for PCs, Steam Deck and Android. Pick it in the web console. [Hosts →](docs/hosts.md)
- **Set-and-forget pairing.** It remembers up to five controllers and reconnects to whichever one you turn on. If a controller drops out, every input is released at once, so nothing stays "pressed".

**Make it yours**
- **Button mapping, per controller.** Swap or move any button, turn a stick into a d-pad (or the other way round), make a trigger a button, invert or scale an axis, set dead zones, and keep one profile per controller model. [Button mapping →](docs/button-mapping.md)
- **Press to pick.** *Learn* fills in a rule when you press the control on the controller. Presets cover the common fixes (Nintendo A/B, invert Y, swap sticks…), and the live view shows the controller's input before and after the map, side by side.

**Keep it running**
- **A web console in your browser.** Status, paired controllers by name, settings and the button maps, over USB from desktop Chrome or Edge. Nothing to install. [Web console →](docs/web-console.md)
- **One-click firmware updates** from the same page, with a safety net: a new firmware runs on trial until you confirm it, and otherwise rolls back by itself. [Updating →](docs/updating.md)
- **Readable crash reports.** If the firmware ever crashes, the dongle keeps the crash dump, and the console downloads it for a bug report.
- **Quiet when idle.** While it waits for a controller, the radio scans at a low duty cycle and the LED barely glows. The T-Dongle-S3's screen shows the link state at a glance.

### See it in action

https://github.com/user-attachments/assets/a0789d38-bd0e-4215-bf2c-ebedd9958495

https://github.com/user-attachments/assets/c81b947a-24a1-4a44-b5d0-5d4c274beb93

<!-- screenshot grid: the v4 web console's Device tab, Button mapping tab (live input, raw vs mapped) and Settings tab -->

## Quick start

You need a [LilyGo T-Dongle-S3](https://lilygo.cc/products/t-dongle-s3) (the
board the releases are built for), a Bluetooth LE controller, and a Windows,
macOS or Linux computer for the one-time install.

1. **Program the dongle once.** Download the programmer for your computer from the
   [latest release](https://github.com/finger563/esp-usb-ble-hid/releases/latest)
   (`esp-usb-ble-hid_programmer_<version>_windows.exe`, `…_macos.bin` or
   `…_linux.bin`), plug in the dongle and run it.
   [Step by step, and what to do if it can't find the dongle →](docs/getting-started.md#install-the-firmware)
2. **Set up the host.**
   - **Nintendo Switch:** *System Settings → Controllers and Sensors → Pro Controller Wired Communication → On.*
   - **PC, Steam Deck or Android:** open the [web console](https://finger563.github.io/esp-usb-ble-hid/dongle_console.html) in Chrome or Edge, click *Connect dongle*, set *Settings → USB controller type* to **Xbox 360**, apply, and reboot the dongle from the Device tab. [Details →](docs/hosts.md#switch-to-xbox-360-mode)
3. **Plug it in and pair.** A new dongle is already in pairing mode (fast blue pulse). Put your controller in pairing mode. On an Xbox controller, hold the small pair button on top until the logo flashes fast. From then on, the dongle reconnects by itself. [Pairing →](docs/getting-started.md#pair-a-controller)

To pair another controller later, hold the dongle's button for 3 seconds. It
then accepts a new controller for 30 seconds.

> [!NOTE]
> **Upgrading from v3?** Open the [web console](https://finger563.github.io/esp-usb-ble-hid/dongle_console.html).
> It offers a one-time **Full update** that adds the storage for button maps.
> Your paired controllers and settings are kept, and your v3 swap / invert /
> dead-zone settings become the default button map.
> [Full update →](docs/updating.md#full-update)

## Everyday use

| The dongle shows | It means | What to do |
|---|---|---|
| LED pulsing blue, fast | **Pairing**: it bonds with the first Bluetooth LE gamepad it finds | Put the controller in pairing mode |
| LED pulsing, slow and dim | **Reconnecting**: looking for a controller it knows | Turn the controller on |
| LED steady and dim | **Connected** to a controller | Play (no input? see [Troubleshooting](docs/troubleshooting.md)) |
| Hold the button 3 s | Start pairing (also in the web console) | |

LED levels, the BLE name and the USB controller type are settings in the
[web console](docs/web-console.md#settings). For other boards, see
[LED and button](docs/hardware.md#led-and-button).

## Good to know

- **Bluetooth LE controllers only.** Controllers that only speak Bluetooth Classic (DualShock 4, DualSense, Switch Pro Controller, Joy-Con) cannot pair, because the ESP32-S3 has no Bluetooth Classic radio. [How to tell →](docs/controllers.md#compatibility)
- **One controller per dongle.** For local multiplayer, use one dongle per player.
- **Rumble both ways.** The host's rumble (the Switch's, or a PC's X-Input rumble in Xbox 360 mode) is forwarded to the Xbox Wireless controller's motors. Motion sensors are not forwarded. [What is forwarded →](docs/controllers.md#what-is-forwarded)
- **It cannot wake a sleeping Switch.** The Switch cuts USB power when it sleeps, so wake it with a Joy-Con or the console's own button.
- **Xbox 360 mode is for PCs, Steam Deck and Android**, not for the Switch. macOS has no X-Input driver, so use Switch Pro mode there.

## Supported hardware

| | Supported | Details |
|---|---|---|
| **Boards** | LilyGo T-Dongle-S3 (recommended, prebuilt releases), Adafruit QT Py ESP32-S3, any ESP32-S3 dev board with native USB | Only the T-Dongle-S3 has prebuilt firmware; build the others from source. [Hardware →](docs/hardware.md#boards) |
| **Controllers** | Xbox Wireless Controller over Bluetooth LE; other Bluetooth LE HID gamepads | [Compatibility →](docs/controllers.md#compatibility) |
| **Hosts** | Switch Pro mode: Nintendo Switch, and hosts that accept a wired Pro Controller. Xbox 360 mode: Windows, Linux, Steam / Steam Deck, Android | [Hosts →](docs/hosts.md) |

## Documentation

| Page | What's there |
|---|---|
| [Getting started](docs/getting-started.md) | Install the firmware, pair a controller, first steps on each host |
| [Hosts](docs/hosts.md) | Switch, PC / Steam Deck / Android (Xbox 360 mode), macOS, Linux permissions |
| [Controllers](docs/controllers.md) | Compatibility, pairing, the default layout, what is forwarded |
| [Button mapping](docs/button-mapping.md) | Profiles, rules, presets, Learn, recipes |
| [Web console](docs/web-console.md) | Every tab and setting, auto-connect |
| [Updating](docs/updating.md) | One-click updates, rollback, the Full update, command line |
| [Troubleshooting and FAQ](docs/troubleshooting.md) | Symptom-first fixes, FAQ, reporting a bug |
| [Hardware](docs/hardware.md) | Boards, LED and button, low-power idle, download mode |
| [Building](docs/building.md) | Build for your board, configuration options, tests |
| [How it works](docs/architecture.md) | Data flow, source map, console modules, espp components |

The web console's own notes (protocol, tests, the vendored flasher) are in
[`web/README.md`](web/README.md).

## For developers

The firmware builds with **ESP-IDF v6.1**, and every library comes from the
[ESP Component Registry](https://components.espressif.com) on the first build:

```sh
git clone https://github.com/finger563/esp-usb-ble-hid
cd esp-usb-ble-hid
idf.py menuconfig          # Hardware Configuration → Target Hardware
idf.py build
```

The first flash, other boards, every configuration option and the tests are in
[Building](docs/building.md#build-for-your-board).

The dongle is also a complete example of an [espp](https://github.com/esp-cpp/espp)
device with a browser console:
- a composite USB device (`usb_device`, `switch_pro`, `xinput`);
- a runtime HID gamepad decoder (`hid-rp`);
- button mapping (`gamepad_map`);
- OTA, core-dump, system and monitor services on one WebUSB stream (`stream_frame` + `dispatcher`).

Adding a console module of your own takes one protocol header, one class and one tab.
[How it works →](docs/architecture.md)

## Credits and references

Built on [espp](https://github.com/esp-cpp/espp) (1.4.6 or newer),
[esp-nimble-cpp](https://github.com/h2zero/esp-nimble-cpp) and
[esp_tinyusb](https://components.espressif.com/components/espressif/esp_tinyusb).
The web console's Full update uses
[esptool-js](https://github.com/espressif/esptool-js). The Pro Controller
implementation owes a great deal to the community's reverse-engineering work:

- [dekuNukem/Nintendo_Switch_Reverse_Engineering](https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering): [USB HID notes](https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/USB-HID-Notes.md), [subcommands](https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/bluetooth_hid_subcommands_notes.md), [SPI flash](https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/spi_flash_notes.md)
- [Brikwerk/nxbt](https://github.com/Brikwerk/nxbt/blob/master/nxbt/controller/protocol.py)
- [mzyy94/nscon](https://github.com/mzyy94/nscon/blob/master/nscon.go) and the [USB gadget write-up](https://www.mzyy94.com/blog/2020/03/20/nintendo-switch-pro-controller-usb-gadget/)
- [EasyConNS/BlueCon-esp32](https://github.com/EasyConNS/BlueCon-esp32/tree/master/components/joycon)

Licensed under the [MIT License](LICENSE).
