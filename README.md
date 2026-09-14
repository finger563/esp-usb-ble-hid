# ESP USB BLE HID

Example code for using BLE gamepad (such as Xbox wireless controller) with the
Nintendo Switch via a USB dongle.

This repository contains example code for using an ESP32s3 to act as a USB-BLE
HID bridge. You would run this code for instance on a QtPy ESP32s3 or a LilyGo
T-Dongle S3, connected to a computer or other device which is a USB HID host.
The main HID host that this targets is the Nintendo Switch. The QtPy / this code
would then start a BLE GATT Client to connect to a BLE HID device (this example
targets a gamepad), and will allow the wireless HID device (gamepad) to talk to
the HID Host.

![image](https://github.com/user-attachments/assets/d76558db-c34e-48d4-9771-06fa4ebdb05a)

<!-- markdown-toc start - Don't edit this section. Run M-x markdown-toc-refresh-toc -->
**Table of Contents**

- [ESP USB BLE HID](#esp-usb-ble-hid)
  - [Plug and Play](#plug-and-play)
    - [Purchase Dongle](#purchase-dongle)
    - [Program It](#program-it)
    - [Plug it into your Switch](#plug-it-into-your-switch)
  - [Cloning](#cloning)
  - [Configuration](#configuration)
  - [Build and Flash](#build-and-flash)
  - [How To Use](#how-to-use)
    - [Pairing Mode](#pairing-mode)
    - [Reconnection Mode](#reconnection-mode)
    - [Connected](#connected)
    - [Note about system power](#note-about-system-power)
  - [Dongle Console (settings, firmware update, crash dumps)](#dongle-console-settings-firmware-update-crash-dumps)
    - [Settings](#settings)
    - [Firmware update (OTA over USB)](#firmware-update-ota-over-usb)
    - [Crash dumps](#crash-dumps)
    - [Adding your own module](#adding-your-own-module)
  - [Architecture](#architecture)
  - [Output](#output)
  - [Helpful Links](#helpful-links)

<!-- markdown-toc end -->

## Plug and Play

If you just want to get a dongle and use your BLE controller with the switch,
simply get the dongle, plug it into your computer, and run the programmer
executable from the release.

### Purchase Dongle

Sources:
- [LilyGo](https://lilygo.cc/products/t-dongle-s3?srsltid=AfmBOopsToYDfOeA4GJiUlQNNcefgA_lMLmWoF99lzdWc_j5Ysd9FUeW)
- [Amazon](https://www.amazon.com/LILYGO-T-Dongle-S3-ESP32-S3-Development-Display/dp/B0BK9162QY)

### Program It

The dongle will require one-time programming to function as a BLE HID USB
dongle.

Download the release `programmer` executable from the latest [releases
page](https://github.com/finger563/esp-usb-ble-hid/releases) for `windows`,
`macos`, or `linux` - depending on which computer you want to use to perform the
one-time programming.

1. Download the programmer
2. Unzip it
3. Double click the `exe` (if windows), or open a terminal and execute it from
   the command line `./esp-usb-ble-hid_programmer_v2.0.2_macos.bin`.

### Plug it into your Switch

Now that the dongle is programmed, simply plug it into your Switch and turn your
switch on.
  
See the [How To Use](#how-to-use) section for information about how to pair /
reconnect your controller to the dongle.

https://github.com/user-attachments/assets/a0789d38-bd0e-4215-bf2c-ebedd9958495

https://github.com/user-attachments/assets/c81b947a-24a1-4a44-b5d0-5d4c274beb93


## Cloning

``` sh
git clone https://github.com/finger563/esp-usb-ble-hid
```

All library code comes from the [ESP Component
Registry](https://components.espressif.com) (the
[espp](https://github.com/esp-cpp/espp) components, `esp_tinyusb`, ...) and is
fetched by the IDF component manager on the first build. The project builds
with **ESP-IDF v6.1** (what CI uses).

## Configuration

You can run `idf.py menuconfig` to configure the project to run on either the
`T-Dongle-S3` or the `QtPy (ESP32 or ESP32S3)`. The configuration is under the
`Hardware Configuration` menu from the main menu and is the `Target Hardware`
option.

The `USB Configuration` menu controls the extra USB interfaces next to the
Switch Pro HID interface:

- **Vendor (WebUSB) interface** (default on) — carries the [dongle
  console](#dongle-console-settings-firmware-update-crash-dumps) stream
  (settings, firmware update, crash dumps). Turn it off to present a plain
  single-interface HID gamepad if a host refuses the composite device.
- **CDC-ACM log console** (default off) — a USB serial port carrying the logs,
  so `idf.py monitor` works over the same cable.

![CleanShot 2025-04-10 at 07 57 26](https://github.com/user-attachments/assets/be355584-251d-4c2c-81ed-15089b45f4e1)

## Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(Replace PORT with the name of the serial port to use.)

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## How To Use

> NOTE: you must turn on the `support wired controllers` setting on your switch
> for the dongle (or any wired controllers for that matter) to work.

The dongle can store up to 5 paired devices at a time. When it turns on / is
plugged in it will attempt to reconnect to one of those devices. If there are no
paired devices, then it will enter pairing mode.

If at any time you want to pair a new controller, simply press and hold the
button on the dongle until the LED starts pulsing blue.

### Pairing Mode

While in pairing mode, the device will scan for any BLE devices which expose a
HID service. It will connect and attempt to bond to the first device it finds.

### Reconnection Mode

When in this mode, the device will scan for the devices in its pairing list and
connect to the first one it finds.

### Connected

While connected, the device will translate xbox controller inputs received via
BLE into Nintendo Switch Pro controller inputs which will then be transmitted
over USB.

If the controller disconnects, then the dongle will re-enter reconnection mode.

### Note about system power

The switch turns off its USB-C port when it enters sleep mode. This means that
while the Switch Dock's USB-A port still has power, the dongle will not properly
mount as a usb device until the Switch comes out of sleep. 

For this reason, you cannot use this dongle or the associated BLE controller to
power on your switch unfortunately. The only way (currently) to remotely wake
your switch is via Bluetooth Classic.

That being said, I have read online that if you plug a usb-to-ethernet adapter
into your Switch Dock, then the Switch may keep its USB-C port awake during
sleep.

## Dongle Console (settings, firmware update, crash dumps)

Plug the dongle into a computer and open the **[Dongle
Console](https://finger563.github.io/esp-usb-ble-hid/dongle_console.html)** in
a Chromium-based browser (Chrome / Edge / Brave — it uses WebUSB, so it also
works from a local copy of [`web/dongle_console.html`](web/dongle_console.html)
opened via `file://`). Click *Connect* and pick the *Pro Controller* device. No
driver is needed on any OS (the dongle advertises WebUSB + MS OS 2.0
descriptors).

The console talks to the dongle over a USB **vendor interface** that sits next
to the gamepad HID interface, using the espp `stream_frame` framing and
`dispatcher` module routing. Three modules are available:

| Module | Id | What it does |
|--------|----|--------------|
| Device Config | `0x10` | status, settings, pairing / forget controllers / reboot (this repo: `components/device_config`) |
| OTA | `0x00` | firmware update over USB (`espp/ota`) |
| Core Dump | `0x04` | download / erase the last crash dump (`espp/coredump`) |

### Settings

Settings are stored in NVS and survive updates:

- **Invert left / right stick Y** (default on — what the Switch expects)
- **Swap A/B**, **Swap X/Y** — use the Xbox physical layout on the Switch
- **Stick deadzone** (0–50 %)
- **LED brightness**
- **BLE name** (applies after a reboot)

Actions: **Start pairing** (same as holding the button), **Forget all
controllers**, **Reboot**. The status card shows USB / BLE state, the connected
controller's serial and battery, the number of paired controllers, uptime and
the firmware / hardware / IDF versions. The **Paired controllers** card lists
every bonded controller by the name it reports (its BLE Device Name, read and
remembered each time it connects — "Unknown controller" until then), with its
address, which one is connected, and a per-controller *Forget*.

### Firmware update (OTA over USB)

The *Firmware* tab flashes a `build/esp-usb-ble-hid.bin` (from the
[releases](https://github.com/finger563/esp-usb-ble-hid/releases) or your own
build) over the vendor interface — no bootloader mode, no serial port. The
partition table has two app slots; after an update the new image boots in
*pending-verify* state and the console asks you to **confirm** it once it
reconnects (or roll back). If it is never confirmed, the bootloader returns to
the previous firmware on the next reset.

From the command line, the same protocol is driven by the espp OTA tool:
`idf.py ota-usb` (after a build; set `ESPP_OTA_VID=0x057E ESPP_OTA_PID=0x2009`
since the dongle presents as a Pro Controller).

> The partition layout changed with this feature (factory → `ota_0`/`ota_1`).
> Dongles running an older release must be reflashed once over serial / with
> the release programmer; after that, updates go over USB.

### Crash dumps

If the firmware ever crashes, the panic handler writes a core dump to the
`coredump` partition and the next boot logs a summary. The *Crash dump* tab
downloads it as `core.elf` (analyze with `espcoredump.py info_corefile --core
core.elf --core-format elf build/esp-usb-ble-hid.elf`) and can erase it.

### Adding your own module

The console stream is the espp dispatcher, so any custom protocol can be added
as another module: see `components/device_config` for a complete, host-tested
example (protocol header + module class + web UI) and the espp [custom modules
guide](https://esp-cpp.github.io/espp/dispatcher/custom_modules.html).

## Architecture

```
  BLE gamepad ──notify──▶ Xbox (hid-rp parse) ──▶ GamepadInputs ──settings──▶ espp::SwitchPro
                                                                                  │ input report
                                                              espp::UsbDevice ◀───┘
                                                          ┌────────┴──────────────────┐
                                             HID iface (Pro Controller)     vendor iface (WebUSB)
                                                     │                              │
                                              Nintendo Switch              espp::Dispatcher
                                                                        ┌──────┼──────────┐
                                                                     OTA   Core Dump   Device Config
```

- `main/usb.cpp` — the composite USB device (HID + optional vendor + optional
  CDC), the HID handshake/report sender task and the vendor RX worker.
- `main/services.cpp` — NVS settings + the OTA / core-dump / device-config
  modules on the dispatcher.
- `main/ble.cpp` — BLE central: scanning, pairing, bonding, HID subscription.
- `components/device_config` — the configuration protocol + module (host tests
  in `test/`).
- `web/dongle_console.html` — the browser console (published to GitHub Pages).

## Output

https://github.com/user-attachments/assets/a0789d38-bd0e-4215-bf2c-ebedd9958495

![CleanShot 2025-02-25 at 08 54 49](https://github.com/user-attachments/assets/d06a53cb-c20c-4de8-9987-38a7bc05b60a)

![CleanShot 2025-02-25 at 09 02 40](https://github.com/user-attachments/assets/6c3820d1-b9f0-4188-96a6-0d1d8b44e1fb)

![CleanShot 2025-02-25 at 09 03 03](https://github.com/user-attachments/assets/89f524e4-1737-4aec-92ac-e3a64f69c6fe)

## Helpful Links

The links below were invaluable in developing the switch pro implemenation
within this repo such that it would work on MacOS, Android, iOS, and (most
importantly) the Nintendo Switch.

* https://github.com/Brikwerk/nxbt/blob/master/nxbt/controller/protocol.py
* https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/bluetooth_hid_subcommands_notes.md
* https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/USB-HID-Notes.md
* https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/spi_flash_notes.md
* https://github.com/EasyConNS/BlueCon-esp32/tree/master/components/joycon
* https://github.com/mzyy94/nscon/blob/master/nscon.go
* https://www.mzyy94.com/blog/2020/03/20/nintendo-switch-pro-controller-usb-gadget/

