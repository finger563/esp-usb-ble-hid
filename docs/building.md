# Building from source

This page covers building the firmware with ESP-IDF for any supported board, every build option, the first flash and the day-to-day development loop on a native-USB board, the tests, and what CI checks. You need a source build for any board other than the LilyGo T-Dongle-S3, and for any change to the firmware.

## What you need

- **ESP-IDF v6.1.** CI builds with v6.1, and `dependencies.lock` records ESP-IDF 6.1.0. `main/idf_component.yml` accepts `idf >= 5.4`, but no older version is built or tested.
- **A git clone, not a source archive.** The firmware's version string comes from `git describe`, for example `v4.0.0` or `v4.0.0-3-gabc1234`. The web console shows it and compares it with the latest release. A build outside a git checkout gets the placeholder version `1`.
- **Network access for the first build.** The ESP-IDF component manager downloads every dependency listed in `main/idf_component.yml`: the espp components, `espressif/esp_tinyusb`, and through them `h2zero/esp-nimble-cpp`, `joltwallet/littlefs` and `lvgl/lvgl`. `dependencies.lock` pins the exact versions CI uses. There are no git submodules.
- **For `idf.py ota-usb`:** the `pyusb` Python package and libusb. See [Update over USB without the button](#update-over-usb-without-the-button).
- **For the tests:** a host C++20 compiler and Node.js. Neither test needs ESP-IDF.

## Get the source

```sh
git clone https://github.com/finger563/esp-usb-ble-hid
cd esp-usb-ble-hid
```

Run the `idf.py` commands below from this folder, in a shell where ESP-IDF's `export.sh` (or `export.ps1` / `export.bat` on Windows) has been run.

## Build for your board

| Board | Configure | Then |
|---|---|---|
| LilyGo T-Dongle-S3 | nothing (it is the default) | `idf.py build` |
| Adafruit QT Py ESP32-S3 | `idf.py menuconfig`, then *Hardware Configuration → Target Hardware → Adafruit QT Py ESP32-S3* | `idf.py build` |
| Generic ESP32-S3 dev board | `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.generic_s3" set-target esp32s3` | `idf.py build` |

`sdkconfig.defaults` already sets the target to `esp32s3`, so a fresh clone needs no `set-target` for the first two boards. For board details (pins, LEDs, what a generic board needs), see [Boards](hardware.md#boards).

### LilyGo T-Dongle-S3

```sh
idf.py build
```

This is the same configuration CI builds and releases. In menuconfig the board is listed as **LilyGo T3 Dongle**.

### Adafruit QT Py ESP32-S3

```sh
idf.py menuconfig    # Hardware Configuration → Target Hardware → Adafruit QT Py ESP32-S3
idf.py build
```

### Generic ESP32-S3 dev board

```sh
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.generic_s3" set-target esp32s3
idf.py menuconfig    # optional: Hardware Configuration → the WS2812 status LED GPIO
idf.py build
```

The overlay, `sdkconfig.defaults.generic_s3`, selects the generic board and sets `CONFIG_DONGLE_DEFAULT_XBOX360=y`, so a fresh board starts as an Xbox 360 (X-Input) controller for a PC. The LED GPIO defaults to 48 (ESP32-S3-DevKitC-1 v1.0). Use 38 for a DevKitC-1 v1.1, or `-1` for a board with no addressable LED.

### Change board or overlay later

`sdkconfig` is generated once, from `sdkconfig.defaults` plus any overlay, and it is not tracked by git. After that, a plain `idf.py build` keeps using it, so passing a different `SDKCONFIG_DEFAULTS` to `build` changes nothing.

To start over with another board or overlay, run `set-target` again. It renames the old `sdkconfig` to `sdkconfig.old`, cleans the build folder and regenerates `sdkconfig` from the defaults you pass:

```sh
# back to the default (T-Dongle-S3) configuration
idf.py set-target esp32s3
# or to the generic overlay
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.generic_s3" set-target esp32s3
```

You can also change the board in menuconfig at any time. Menuconfig only changes the board option, though, and does not apply the overlay's other setting (`CONFIG_DONGLE_DEFAULT_XBOX360`).

## Configuration options

These are all the options in `main/Kconfig.projbuild`. In `idf.py menuconfig` they sit in two top-level menus, **Hardware Configuration** and **USB Configuration**.

| Option | Prompt in menuconfig | Default | What it does |
|---|---|---|---|
| `CONFIG_TARGET_HARDWARE_T3_DONGLE` | Target Hardware → *LilyGo T3 Dongle* | selected | Builds for the LilyGo T-Dongle-S3: screen, APA102 LED, BOOT button. |
| `CONFIG_TARGET_HARDWARE_QTPY_ESP32_S3` | Target Hardware → *Adafruit QT Py ESP32-S3* | – | Builds for the QT Py ESP32-S3: NeoPixel, BOOT button, no screen. |
| `CONFIG_TARGET_HARDWARE_GENERIC_S3` | Target Hardware → *Generic ESP32-S3 dev board (BOOT button, optional WS2812 LED)* | – (selected by the generic overlay) | Builds for any ESP32-S3 board: BOOT (GPIO0) as the pairing button, optional WS2812, no screen. |
| `CONFIG_GENERIC_S3_LED_GPIO` | *Generic ESP32-S3: WS2812 status LED GPIO (-1 = no LED)* | `48` | The WS2812's data GPIO. Only shown for the generic board. Use `38` for a DevKitC-1 v1.1 or `-1` for no LED. |
| `CONFIG_DONGLE_USB_VENDOR_INTERFACE` | *Add a vendor (WebUSB) interface for the dongle console* | `y` | Adds the vendor interface that carries the web console, firmware updates over USB, crash-dump download, button-map editing and settings. Turn it off to present a plain gamepad, for example to a host that refuses a composite device. See [Builds without the vendor interface](#builds-without-the-vendor-interface). |
| `CONFIG_DONGLE_DEFAULT_XBOX360` | *Present as an Xbox 360 (X-Input) controller on first boot* | `n` (`y` in the generic overlay) | Sets the first-boot value of the *USB controller* setting. The setting is stored in NVS on the first boot, so this option only affects a board with no stored settings. Flashing keeps NVS, so on a board that already ran the firmware, change the setting on the web console's [Settings tab](web-console.md#settings) instead. The console's *Reset to defaults* restores this first-boot value too. |
| `CONFIG_DONGLE_USB_XBOX_PID` | *USB product ID in Xbox 360 mode (vendor ID 0x045E)* | `0x0E60` | The USB product ID in Xbox 360 mode. It must not be one that Windows' Xbox 360 driver lists by hardware ID, such as the retail `0x028E` (see [the Xbox 360 identity](architecture.md#the-xbox-360-identity)). If you change it, change `XBOX_PID` in `web/dongle_console.html` to match, because the console only opens identities it knows on its own. Use the new value for `ESPP_OTA_PID` and in your udev rule too. These are Microsoft's identifiers, so use them only to emulate your own device. |
| `CONFIG_DONGLE_USB_CDC_CONSOLE` | *Add a CDC-ACM interface carrying the log console* | `n` | Adds a USB serial port that carries the log console, so `idf.py monitor` works over the same cable. It is off by default to keep the USB descriptor minimal for the Switch, so release builds do not have it. See [See the logs](#see-the-logs). |

### Builds without the vendor interface

With `CONFIG_DONGLE_USB_VENDOR_INTERFACE=n` the dongle is a plain controller, and these things change:

- **The web console cannot connect.** You cannot update the firmware over USB, download crash dumps, edit button maps or change settings. The dongle still applies the settings and button maps already stored on it.
- **Updates need download mode.** Flash new firmware over serial, holding BOOT while you plug the board in (see [Download mode](hardware.md#download-mode)).
- **Switch Pro mode** presents the Pro Controller HID interface, plus the CDC log port if you enabled it.
- **Xbox 360 mode** presents the X-Input interface alone, with the retail Xbox 360 controller identity (`045E:028E`) instead of `CONFIG_DONGLE_USB_XBOX_PID`, and without the CDC log port even when it is enabled. The reason is explained under [the Xbox 360 identity](architecture.md#the-xbox-360-identity).

### Other build settings

`sdkconfig.defaults` also sets these. You rarely need to change them:

| Setting | Value | Why |
|---|---|---|
| `CONFIG_ESPTOOLPY_FLASHSIZE_4MB` | 4 MB | The flash layout in `partitions.csv` (see [Flash layout](architecture.md#flash-layout)). |
| `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME` | `partitions.csv` | Two OTA app slots, a core-dump partition and a LittleFS partition for the button maps. |
| `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` | `y` | A newly installed image must be confirmed, or it is rolled back on the next reset. See [Confirm or roll back](updating.md#confirm-or-roll-back). |
| `CONFIG_BT_NIMBLE_ENABLED` | `y` (Bluedroid off) | The Bluetooth LE host is NimBLE, used through esp-nimble-cpp. |
| `CONFIG_BT_NIMBLE_MAX_BONDS` / `CONFIG_BT_NIMBLE_MAX_CCCDS` | `5` / `128` | How many controllers the dongle remembers. `MAX_CCCDS` must be at least 6 × `MAX_BONDS`, or it becomes the limit, so raise it too if you go above 21 bonds. |
| `CONFIG_BT_NIMBLE_NVS_PERSIST` | `y` | Pairings are stored in NVS and survive reboots and updates. |
| `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH` | `y` | A crash writes a core dump to the `coredump` partition, which the web console can download. |
| `CONFIG_FREERTOS_USE_TRACE_FACILITY`, `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS` | `y` | The task table that the espp monitor service reports. |
| `CONFIG_TINYUSB_VENDOR_RX_BUFSIZE` / `TX_BUFSIZE`, `CONFIG_TINYUSB_CDC_ENABLED` | `4096`, `y` | Buffers for the console stream. The CDC class must be compiled in for espp `usb_device` even when no CDC port is configured. |
| `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ` | `240` | CPU clock. Power management is not enabled. |

Two compile-time debug switches live at the top of `main/main.cpp`, not in Kconfig. See [Test the USB side without a controller](#test-the-usb-side-without-a-controller).

## First flash and the development loop

On these boards the USB connector goes straight to the ESP32-S3's native USB. While the firmware runs, that connector belongs to the dongle's own composite USB device (the controller, the console interface and, optionally, the CDC log port). The serial port you flashed through disappears once the firmware starts its USB device.

> [!TIP]
> The vanishing serial port is expected. Use download mode or `idf.py ota-usb` for the next flash, and the CDC option or UART0 for logs.

### Flash a board the first time

1. Put the board into download mode: hold BOOT while you plug it in (see [Download mode](hardware.md#download-mode)). A USB serial port appears, typically `/dev/cu.usbmodem…` on macOS, `/dev/ttyACM…` on Linux or `COM…` on Windows.
2. Flash it:

   ```sh
   idf.py -p PORT flash
   ```

   This writes the bootloader (`0x0`), the partition table (`0x8000`), the OTA data (`0xf000`) and the app (`0x20000`). It does not erase the rest of the flash, so pairings, settings, button maps and any stored crash dump survive.
3. esptool resets the board into the new firmware when it finishes. If the board stays in download mode, unplug it and plug it back in.

A board already running the firmware can also enter download mode without the button. Click *Reboot into bootloader* on the web console's System tab, then run step 2.

### See the logs

The firmware logs to UART0 at 115200 baud (the ESP-IDF default console: TX GPIO43, RX GPIO44). You can read them in two ways:

- **Over the same USB cable.** Turn on `CONFIG_DONGLE_USB_CDC_CONSOLE` (*USB Configuration → Add a CDC-ACM interface carrying the log console*) and flash again. The dongle then adds a USB serial port that carries the log console. The logs still go to UART0 as well. This port belongs to the running firmware, not to the ROM, so its name can differ from the port you flashed through. Open it with:

  ```sh
  idf.py -p CDC_PORT monitor
  ```

  The CDC port is left out in Xbox 360 mode when the vendor interface is turned off.
- **On UART0.** Use a USB-to-UART adapter on GPIO43/44, or the board's second USB connector if it has one wired to a USB-to-UART bridge (on the ESP32-S3-DevKitC-1, the connector labelled UART).

`idf.py monitor` decodes panic backtraces against `build/esp-usb-ble-hid.elf`. Crash dumps stored in flash are covered in [Troubleshooting](troubleshooting.md#reporting-a-bug).

### Update over USB without the button

Once a board runs a build with the vendor interface, `idf.py ota-usb` (from the espp `ota` component) builds the app and installs it over the same USB connection, with no button and no serial port. Pass the identity the dongle has right now:

```sh
# a dongle in Switch Pro mode
ESPP_OTA_VID=0x057E ESPP_OTA_PID=0x2009 idf.py ota-usb

# a dongle in Xbox 360 mode
ESPP_OTA_VID=0x045E ESPP_OTA_PID=0x0E60 idf.py ota-usb
```

Always set both variables. The tool's own default identity (`0x1209:0x0D32`) is not the dongle's. If you changed `CONFIG_DONGLE_USB_XBOX_PID`, use your value. After the upload the tool waits for the dongle to reboot, reconnects, and confirms the new image for you.

The tool needs `pyusb` with a libusb backend in the ESP-IDF Python environment, and on Linux a udev rule (see [Linux permissions](hosts.md#linux-permissions)). Setup, the standalone `espp-ota` tool, and what to do when a pending *USB controller* change makes the dongle come back under its other identity are covered in *Update from the command line* on the [Updating the firmware](updating.md) page. An image that nobody confirms is rolled back on the next reset (see [Confirm or roll back](updating.md#confirm-or-roll-back)).

The other route is to click *Reboot into bootloader* on the System tab and then run `idf.py -p PORT flash`.

## Tests

Neither test needs ESP-IDF or hardware. CI does not run them yet, so run both before you open a pull request.

### Device-config protocol (C++)

`components/device_config/include/detail/device_config_protocol.hpp` uses only the standard library, so its encoders and parsers build on your computer:

```sh
c++ -std=c++20 -Wall -Wextra -Werror -I components/device_config/include \
    components/device_config/test/device_config_host_test.cpp -o dc_test && ./dc_test
```

It prints `ALL DEVICE CONFIG PROTOCOL TESTS PASSED`. It round-trips the settings, INFO and bond-list payloads. It also checks partial settings updates, value validation, that unknown keys are ignored, version gating, and that malformed payloads are rejected.

### Web console (Node.js)

```sh
node web/test/dongle_console_test.js
```

The test has no dependencies. It checks the console's pure logic against byte vectors derived from the C++ headers: the `espp.gamepad_map` codec block (with its rumble options and the rumble test), the Full update planner (manifest, partition-table and image checks, MD5, the esptool-js read fix), the INFO `needs_full_flash` flag, the settings payload, and that every inline script parses. It ends with `ALL PASS`. How the console's code is laid out for this is described in the [console protocol notes](../web/README.md#testing-the-protocol-code).

### Test the USB side without a controller

Two `#define`s at the top of `main/main.cpp` push made-up inputs while no controller is connected:

- **`DEBUG_NO_BLE_TWIRL_JOYSTICKS 1`** moves both sticks around a circle, one step a second. The LED turns red while USB is not mounted.
- **`DEBUG_NO_BLE_TEST_BUTTONS 1`** also presses the buttons one after another. It only works with the first switch on. The source warns that it gets annoying quickly.

Set them back to `0` before you commit.

### Before you open a pull request

- Run both tests above.
- Build every board configuration your change touches. CI builds only the default (T-Dongle-S3) configuration.
- Format C and C++ with clang-format. The repository has a `.clang-format` and a pre-commit hook pinned to clang-format 14.0.6 (`pre-commit run --all-files`).
- If you change the gamepad-map codec block in `web/dongle_console.html`, follow the re-sync rule in the [console protocol notes](../web/README.md#testing-the-protocol-code). The block is a verbatim copy of espp's.

## What CI runs

| Workflow | Runs on | What it does |
|---|---|---|
| `build.yml` (*Build*) | pull requests | Builds the default configuration with ESP-IDF v6.1 for `esp32s3` (`espressif/esp-idf-ci-action`). Then it reports the app size change against one OTA slot (`0x1D0000` bytes). The size step is informational and allowed to fail. |
| `static_analysis.yml` (*Static analysis*) | pull requests | Runs cppcheck through `esp-cpp/StaticAnalysis`, skipping `components/gui/generated`, with `suppressions.txt`. |
| `package_main.yml` (*Build and Package Main*) | pushes to `main`, published releases, manual | Builds the default configuration and uploads the images as the `build-artifacts` artifact. For a release it attaches the `.bin` and `.elf` files, the bootloader, the partition table, `flasher_args.json` and `flash_args` to the release. It then packages one-file programmers for Windows, macOS and Linux with `esp-cpp/esp-packaged-programmer-action`. |
| `publish_webapp.yml` (*Publish Dongle Console*) | pushes to `main` that touch `web/`, after a successful release build, manual | Publishes `web/` (without `web/test`) to GitHub Pages. Next to the console it adds `firmware/latest/`: the latest release's flash images and a `manifest.json`. |

CI never builds the QT Py or generic configurations, and no release contains firmware for them. How users install the programmer is covered in [Install the firmware](getting-started.md#install-the-firmware). How the console uses `firmware/latest/` is covered in [Updating the firmware](updating.md).
