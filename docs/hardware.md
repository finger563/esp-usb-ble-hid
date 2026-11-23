# Hardware

This page covers the boards the firmware runs on, the button and status LED, the T-Dongle-S3's screen, how the dongle keeps idle power low, and how to get a board into the ESP32-S3's download mode.

## Boards

The firmware supports three board targets. You pick one when you build (see [Build for your board](building.md#build-for-your-board)). Only the T-Dongle-S3 has prebuilt firmware.

| Board | Kconfig choice | Pairing button | Status light | Screen | Prebuilt firmware |
|---|---|---|---|---|---|
| **[LilyGo T-Dongle-S3](https://lilygo.cc/products/t-dongle-s3)** (recommended) | `TARGET_HARDWARE_T3_DONGLE` (the default) | BOOT (GPIO0) | APA102 RGB LED | 160 × 80 colour LCD | Yes, in every release |
| **[Adafruit QT Py ESP32-S3](https://www.adafruit.com/product/5426)** | `TARGET_HARDWARE_QTPY_ESP32_S3` | BOOT (GPIO0) | NeoPixel (data GPIO39, power GPIO38) | None | No, build from source |
| **Generic ESP32-S3 dev board** (for example an ESP32-S3-DevKitC-1 or a clone) | `TARGET_HARDWARE_GENERIC_S3`, through the `sdkconfig.defaults.generic_s3` overlay | BOOT (GPIO0) | Optional WS2812 on `CONFIG_GENERIC_S3_LED_GPIO` | None | No, build from source |

The firmware reports which build it is. The web console's Device tab shows it in the Hardware row as `LilyGo T-Dongle-S3`, `Adafruit QT Py ESP32-S3` or `Generic ESP32-S3` (`HARDWARE_NAME` in `main/bsp.hpp`).

> [!WARNING]
> Releases are built only for the T-Dongle-S3. The web console offers the latest release to any dongle and does not check the board. On a QT Py or a generic board, install your own build instead. See [Updating the firmware](updating.md).

### What a board needs

- **An ESP32-S3.** The firmware needs Bluetooth LE and a USB device (USB-OTG) controller on the same chip, and every board choice in `main/Kconfig.projbuild` depends on `IDF_TARGET_ESP32S3`. The ESP32-S2 has no Bluetooth. The ESP32-C3 and ESP32-C6 have only a USB-Serial-JTAG port and cannot present a custom USB device.
- **A USB connector wired to the S3's native USB pins** (GPIO19 = D−, GPIO20 = D+). The host talks to the dongle through this connector. A board whose only USB connector goes through a USB-to-UART bridge chip cannot work as a dongle.
- **At least 4 MB of flash.** The build uses a 4 MB layout (`CONFIG_ESPTOOLPY_FLASHSIZE_4MB`), and `partitions.csv` fills it up to `0x400000`. A board with more flash works, but the extra space is not used. See [the flash layout](architecture.md#flash-layout).
- **No PSRAM.** The firmware does not enable or use PSRAM.
- **A button on GPIO0** for the generic target. On ESP32-S3 dev boards the BOOT button is wired to GPIO0, the chip's boot-mode pin.

### LilyGo T-Dongle-S3

This is the recommended board and the one the releases target. It plugs straight into a USB-A port, it has the screen described [below](#the-t-dongle-s3-screen), and its single button is the BOOT button (GPIO0). The espp `t-dongle-s3` component drives the APA102 LED (SPI, data GPIO40, clock GPIO39) and the LCD. The board's microSD slot is not used.

In menuconfig, this board is listed as **LilyGo T3 Dongle**.

### Adafruit QT Py ESP32-S3

A smaller board with a NeoPixel status LED and no screen. The espp `qtpy` component drives the NeoPixel (data GPIO39, power enable GPIO38) and reads the BOOT button (GPIO0). Select it in menuconfig before you build.

### Generic ESP32-S3 dev board

Any ESP32-S3 board that meets [the requirements above](#what-a-board-needs). `main/generic_s3.hpp` is its whole board support:

- **The BOOT button (GPIO0) is the pairing button.** It is read active-low with the internal pull-up on.
- **The status LED is an optional WS2812** on `CONFIG_GENERIC_S3_LED_GPIO`. The default is GPIO48, which is what the ESP32-S3-DevKitC-1 v1.0 uses. The DevKitC-1 v1.1 uses GPIO38. Set the option to `-1` if your board has no addressable LED. LED writes then do nothing.
- **There is no screen.**
- **It starts as an Xbox 360 controller.** The overlay sets `CONFIG_DONGLE_DEFAULT_XBOX360=y`, so a fresh board presents itself as an X-Input gamepad for a PC. You can switch it to a Switch Pro Controller later. See [Switch to Xbox 360 mode](hosts.md#switch-to-xbox-360-mode) for how the setting works.

Plug the host into the board's native USB connector. On boards with two connectors, the other one is usually a USB-to-UART bridge on UART0. The ESP32-S3-DevKitC-1, for example, labels its two connectors USB and UART. The firmware's log output goes to UART0, so you can read the logs on that second connector while the native one is in use (see [Building from source](building.md#see-the-logs)).

### Add a board of your own

`main/generic_s3.hpp` is the template. The app needs a `get()` singleton with this small surface:

- `initialize_led()` and `led(espp::Hsv)` / `led(espp::Rgb)`
- `initialize_button(callback)`, where the callback receives press and release events
- with a screen: `initialize_lcd()`, `initialize_display(pixel_count)`, `lcd_width()` and `lcd_height()`. The status screen in `components/gui` is laid out for the T-Dongle-S3's 160 × 80 panel.

Then add a choice to `main/Kconfig.projbuild` and a branch to `main/bsp.hpp` that sets `HAS_DISPLAY`, `HARDWARE_NAME` and the `Bsp` alias. If your board support is a new component, add it to the `COMPONENTS` list in the top-level `CMakeLists.txt`, because the build includes only the components on that list. If the component comes from the ESP Component Registry, also add it to `main/idf_component.yml`.

## LED and button

### The button

**To start pairing, hold the button for 3 seconds.** If you release it sooner, nothing happens. This does the same as *Start pairing* on the web console's Device tab. The dongle then accepts a new controller for about 30 seconds. While no controller is paired, the dongle stays in pairing mode on its own. For the steps on the controller's side, see [Pair a controller](getting-started.md#pair-a-controller).

On all three boards the pairing button is the BOOT button (GPIO0). Holding it while you plug the board in does something different: it starts [download mode](#download-mode).

### What the LED means

The LED is blue in every state. What changes is the pattern and the level.

| What you see | What it means | Default level |
|---|---|---|
| Off | Starting up, or the level for the current state is set to 0, or a generic board with no LED | – |
| Bright blue pulse, once a second | **Pairing.** The dongle connects to the first Bluetooth HID device it finds. | Pairing level 100 % |
| Very faint blue pulse, every 3 seconds | **Reconnecting.** The dongle is looking for a controller it has already paired with. At the default level this pulse is hard to see. | Reconnecting level 1 % |
| Steady blue | **A controller is connected.** The light goes steady as soon as the Bluetooth link is up, while the dongle is still securing the link and subscribing to the inputs. A steady light alone does not mean inputs are flowing. | Connected level 25 % |
| Blue that blinks with every input | **Connected, with *LED blink on every input* turned on.** This is a debugging setting, off by default. The LED toggles between on and off on each input report while USB is connected. | – |

Each level is a share of the overall LED brightness, which defaults to 100 %. You can change all of them, and the blink setting, on the web console's [Settings tab](web-console.md#settings). LED changes apply at once.

If the light is steady but nothing reaches the host, the web console's Link block shows where the link is stuck. See [Troubleshooting](troubleshooting.md).

### The T-Dongle-S3 screen

The screen shows the link status at a glance:

- **Two icons with an arrow between them.** One icon is the controller (the Bluetooth side). The other is the host (the USB side).
- **The host icon shows the USB mode:** the Switch logo in Switch Pro mode, the Xbox logo in Xbox 360 mode. The controller icon is always the Xbox logo, whatever controller is connected.
- **Each icon shows whether its link is up.** While a link is down, its icon is a white silhouette. When the link comes up, the icon shows its colours and a small Bluetooth or USB badge appears under it. The Bluetooth side counts as up once the dongle is subscribed to the controller's inputs. The USB side counts as up once the host has set up the dongle.
- **The text in the middle is the connected controller's serial number**, read from its Bluetooth Device Information service. It is blank when no controller is connected, or when the controller does not report a serial number.

The screen updates once a second. It is an LVGL screen designed in SquareLine Studio. The project is `components/gui/squareline/esp-usb-ble-hid.spj` and the generated code is in `components/gui/generated/`.

<!-- screenshot: photo of a T-Dongle-S3 screen with both links up (coloured Xbox and Switch icons, BT and USB badges, a serial number in the middle) -->

## Low-power idle

When no controller is connected, the dongle saves power in two places:

- **The Bluetooth scan runs at a 10 % duty cycle.** It listens for 30 ms out of every 300 ms. A controller that advertises every few tens of milliseconds is still found quickly. Each scan lasts 5 seconds, and the dongle starts the next one after a short pause.
- **The LED barely glows.** The reconnecting pulse defaults to 1 % of the LED brightness. To turn the LED off completely, set its levels to 0 on the [Settings tab](web-console.md#settings).

The firmware does not put the ESP32-S3 to sleep. Power management is not enabled, and the CPU runs at 240 MHz. The radio and the LED are the only parts it slows down.

## Download mode

Download mode is the ESP32-S3's built-in ROM bootloader. In this mode the board shows up as an Espressif USB serial device: usually `303a:1001`, "USB JTAG/serial debug unit", or sometimes `303a:0009`. The release programmer, `idf.py flash` / esptool and the web console's [Full update](updating.md#full-update) all write the flash through this mode.

A dongle running this firmware has no serial port a flashing tool can use. Its USB connector belongs to the controller device, so you have to put the board into download mode first. There are two ways:

1. **Use the BOOT button.** Unplug the dongle, hold BOOT, plug the dongle back in, then let go. On a board that also has a reset button (RST or EN), you can instead hold BOOT, press and release reset, then let go of BOOT.
2. **Use the web console.** Click *Reboot into bootloader* on the System tab. You don't have to reach the button, which helps when the dongle sits in a Switch dock. The console cannot reconnect to the board while it is in download mode.

The board stays in download mode until it is flashed and reset, or unplugged and plugged back in. Flashing tools normally reset it into the new firmware when they finish. If the board does not come back, unplug it and plug it back in.

On Linux you may need a udev rule for the `303a` device before a flashing tool or the browser can open it. See [Linux permissions](hosts.md#linux-permissions).

What to run once the board is in download mode:

- **Install a release on a T-Dongle-S3:** see [Install the firmware](getting-started.md#install-the-firmware).
- **Flash your own build:** see [First flash and the development loop](building.md#first-flash-and-the-development-loop).
- **Upgrade the flash layout from the browser:** see [Full update](updating.md#full-update).
