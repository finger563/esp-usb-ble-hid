# Getting started

Put the firmware on a LilyGo T-Dongle-S3, pair your controller with it, and plug it into your Switch or PC. You only need a computer for this first setup.

## What you need

- **A LilyGo T-Dongle-S3.** It is the only board with ready-made firmware ([product page](https://lilygo.cc/products/t-dongle-s3)). It has a USB-A plug and a small screen that shows the link status. Other ESP32-S3 boards work too, but you build the firmware for them yourself: see [Boards](hardware.md#boards).
- **A Bluetooth LE controller.** The Xbox Wireless Controller (its Bluetooth LE models) is the controller this project is tested with. Many other Bluetooth LE gamepads are expected to work. PlayStation 4 and 5 controllers, the Switch Pro Controller and Joy-Cons use Bluetooth Classic, so they don't work. Check your controller in the [compatibility table](controllers.md#compatibility).
- **A computer, once.** You need Windows (64-bit), a Mac with Apple silicon (M1 or newer) or 64-bit Linux (x86-64) to run the installer. Later on, desktop Chrome or Edge opens the optional [web console](web-console.md) for settings and updates.
- **Somewhere to plug it in.** The T-Dongle-S3 fits any USB-A port, such as one on a Switch dock or a PC. USB-C-only devices need a USB-C to USB-A adapter (an "OTG" adapter). That covers a Switch in handheld mode, a Switch Lite, a Steam Deck, phones and many laptops.

## Install the firmware

The installer, called the *programmer*, is one file that writes the firmware onto the dongle. You don't need to install anything else.

### 1. Download the programmer

Open the [latest release](https://github.com/finger563/esp-usb-ble-hid/releases/latest). Under **Assets**, download the file for your computer:

| Your computer | File to download |
|---|---|
| Windows (64-bit) | `esp-usb-ble-hid_programmer_<version>_windows.exe` |
| Mac with Apple silicon (M1 or newer) | `esp-usb-ble-hid_programmer_<version>_macos.bin` |
| Linux (x86-64) | `esp-usb-ble-hid_programmer_<version>_linux.bin` |

> [!NOTE]
> The release also has files such as `esp-usb-ble-hid.bin`, `bootloader.bin` and `partition-table.bin`. Those are firmware images for updates and for developers. The programmer already contains everything it needs, so you can ignore them.
>
> The macOS programmer runs only on Apple-silicon Macs. On an Intel Mac, use a Windows or Linux computer instead, or [build the firmware yourself](building.md#build-for-your-board).

### 2. Put the dongle in download mode

The dongle has one button. Hold it down while you plug the dongle into your computer, then let go.

That starts the chip's download mode, the mode the programmer writes through. Download mode is built into the chip, so it is always there: if anything goes wrong later, you can come back to this step and start over. Once this firmware is running, the dongle no longer shows up as a serial port, so you need download mode every time you run the programmer. More about it, including other boards: [Download mode](hardware.md#download-mode).

### 3. Run the programmer

**Windows:** double-click the `.exe`. A console window opens and shows the progress. The window closes by itself as soon as the programmer stops, so if it vanishes before you could read it, run the programmer from a command prompt instead (see below). The programmer isn't code-signed, so Windows SmartScreen may warn you ("Windows protected your PC"). If it does, click **More info**, then **Run anyway**.

**macOS:** open Terminal and run:

```sh
cd ~/Downloads
chmod +x esp-usb-ble-hid_programmer_*_macos.bin
./esp-usb-ble-hid_programmer_*_macos.bin
```

If macOS refuses to open it because it can't verify it (the message mentions the developer, or malware), open **System Settings > Privacy & Security**, scroll down, click **Open Anyway**, and run the last command again.

**Linux:** in a terminal, run:

```sh
cd ~/Downloads
chmod +x esp-usb-ble-hid_programmer_*_linux.bin
./esp-usb-ble-hid_programmer_*_linux.bin
```

If it can't open the serial port ("permission denied"), add the rules in [Linux permissions](hosts.md#linux-permissions) and try again.

If you downloaded more than one version, type the file's full name instead of the `*`.

The programmer finds the dongle by itself, writes the firmware and restarts the dongle. If it can't find the dongle, or picks the wrong device, tell it which port to use with `--port`. Replace the port in these examples with yours, and on Windows the version with the one you downloaded.

**Windows:** find the port under **Ports (COM & LPT)** in Device Manager. Open Command Prompt or PowerShell in your Downloads folder (in File Explorer, type `cmd` in the address bar and press Enter) and run:

```bat
.\esp-usb-ble-hid_programmer_v4.0.0_windows.exe --port COM5
```

**macOS:** list the ports with `ls /dev/cu.usbmodem*`, then run:

```sh
./esp-usb-ble-hid_programmer_*_macos.bin --port /dev/cu.usbmodem1101
```

**Linux:** list the ports with `ls /dev/ttyACM*`, then run:

```sh
./esp-usb-ble-hid_programmer_*_linux.bin --port /dev/ttyACM0
```

<!-- screenshot: the programmer's console window at the end of a successful run -->

### 4. Check that it worked

When the programmer finishes without an error, the dongle restarts with the new firmware and the T-Dongle-S3's screen lights up. On a dongle with no paired controllers, the LED pulses blue about once a second, which means it is ready to pair. (If you reinstalled over this firmware, your paired controllers are kept, and the LED pulses very faintly every 3 seconds while it looks for them.) If nothing happens, unplug the dongle and plug it back in, this time without holding the button.

If something didn't work, see [The programmer can't find the dongle](troubleshooting.md#the-programmer-cant-find-the-dongle).

> [!TIP]
> Is your dongle already running an older version of this firmware? You can update it from the web console instead, and your paired controllers and settings are kept. Moving to v4 takes one [Full update](updating.md#full-update), which also covers dongles on v2.1.0 or older. Running the programmer again also works, and it keeps them too.

## Pair a controller

You can pair wherever the dongle has power: in your computer right after installing it, in a Switch dock or in a PC. The dongle remembers the controller wherever you plug it in later.

1. **Check that the dongle is in pairing mode.** The LED pulses blue quickly, about once a second.
   - A dongle with no paired controllers is always in pairing mode, so a freshly installed one is ready.
   - Otherwise, hold the dongle's button for 3 seconds until the fast pulse starts. The **Start pairing** button in the [web console](web-console.md) does the same.
   - After you hold the button, the dongle accepts a new controller for about 30 seconds. After that it goes back to looking only for controllers it already knows. If you miss the window, hold the button again.
2. **Put your controller in pairing mode.** For an Xbox Wireless Controller, press the Xbox button to turn it on. Then press and hold the small pair button on the top edge, next to the USB port, until the Xbox button flashes quickly. For other controllers, see [Controllers](controllers.md).
3. **Wait a few seconds.** When the controller connects, the LED stops pulsing and glows a steady dim blue. Once the dongle has finished setting up the link and receives the controller's inputs, the controller icon on the T-Dongle-S3's screen changes from a white silhouette to color and a Bluetooth badge appears under it.

From then on, just turn the controller on and it reconnects by itself. The dongle remembers up to five controllers and uses one at a time. The full list of LED patterns is in [LED and button](hardware.md#led-and-button).

> [!TIP]
> In pairing mode the dongle takes the **first** Bluetooth LE controller or other Bluetooth LE input device it finds, and keyboards, mice and remotes count. Before you pair:
> - Turn off other controllers, including ones already paired with the dongle, and anything else that might be in Bluetooth pairing mode.
> - Pair one dongle at a time if you have several. A new dongle is always in pairing mode.

<!-- photo: the dongle's LED pulsing (pairing) next to the steady dim LED (connected) -->

## What to do next

### Nintendo Switch

1. On the Switch, turn on **Pro Controller Wired Communication**. [Nintendo Switch](hosts.md#nintendo-switch) shows where to find it.
2. Plug the dongle into the dock, or into a USB-C adapter in handheld mode.
3. Turn your controller on and play.

The dongle places buttons by position, the way an Xbox controller labels them. If you want Nintendo's A/B positions instead, two ready-made presets in the web console set that up: see [button mapping recipes](button-mapping.md#recipes).

### PC, Steam Deck or Android

The dongle starts out as a Switch Pro Controller, and most PC games expect an Xbox controller. While the dongle is still plugged into your computer, [switch it to Xbox 360 mode](hosts.md#switch-to-xbox-360-mode) in the web console. Then plug it into your PC, Steam Deck or Android device.

### Mac, iPhone or iPad

Keep the dongle in Switch Pro mode. See [macOS and iOS](hosts.md#macos-and-ios).

### Then

- **Remap buttons**, fix stick drift or set up a one-handed layout: [Button mapping](button-mapping.md).
- **Check status**, see your paired controllers and change settings: [Web console](web-console.md).
- **Keep the firmware up to date** from the browser: [Updating](updating.md).
- **Something isn't working?** See [Troubleshooting](troubleshooting.md).
