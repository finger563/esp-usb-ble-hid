# What you can plug it into

The dongle can act as a wired Nintendo Switch Pro Controller or a wired Xbox 360 controller. This page helps you pick the right mode for your device and explains what to expect on each one.

## Choose a mode

The web console calls the mode the **USB controller** setting. The release firmware starts out as a Switch Pro Controller. (A build for a generic ESP32-S3 board starts out in Xbox 360 mode instead.) You change the mode in the [web console](web-console.md#settings) (see [Switch to Xbox 360 mode](#switch-to-xbox-360-mode)), and the dongle remembers it.

| You play on | Mode to use | Notes |
|---|---|---|
| Nintendo Switch | Switch Pro | The Switch doesn't recognize the Xbox 360 mode. See [Nintendo Switch](#nintendo-switch). |
| Windows PC | Xbox 360 | No driver to install. |
| Steam, including the Steam Deck | Xbox 360 | The Steam Deck needs a USB-C adapter or dock. |
| Linux | Xbox 360 | Uses the kernel's `xpad` driver. |
| Android | Xbox 360 | Needs a USB-C (OTG) adapter. |
| Mac, iPhone, iPad | Switch Pro | macOS has no Xbox 360 (X-Input) driver. See [macOS and iOS](#macos-and-ios). |
| Xbox or PlayStation consoles | Not supported | |

What changes between the two modes:

| | Switch Pro Controller | Xbox 360 Controller (X-Input) |
|---|---|---|
| Shows up as | "Pro Controller", USB ID `057E:2009` | "Xbox 360 Controller", USB ID `045E:0E60` |
| Triggers | ZL / ZR, on or off (counted as pressed past halfway) | LT / RT, analog |
| Your controller's Xbox / Home button | Home | Guide |
| Share / Capture button | Capture | Not available (X-Input has no such button) |
| Controller battery level | Sent to the Switch | Not sent |
| Rumble | The Switch's rumble drives the motors of the Xbox Wireless controller (Series X|S, model 1914) | The PC's X-Input rumble drives them |
| Motion controls | Not forwarded | Not forwarded |
| Web console | Works | Works |

For everything the dongle passes on, and what it doesn't, see [What is forwarded](controllers.md#what-is-forwarded).

## Nintendo Switch

In Switch Pro mode, the Switch sees the dongle as a wired Pro Controller. It doesn't know a Bluetooth controller is behind it.

### Turn on wired controllers

The Switch ignores wired Pro Controllers until you allow them. Go to **System Settings > Controllers and Sensors > Pro Controller Wired Communication** and turn it **On**. You only need to do this once.

### Plug it in

- **Docked:** plug the dongle into one of the dock's USB ports.
- **Handheld, tabletop or Switch Lite:** connect the dongle to the Switch's USB-C port through a USB-C to USB-A adapter (an "OTG" adapter).

Leave the dongle in Switch Pro mode, which is what it starts in. If you changed it to Xbox 360 mode for a PC, [change it back](#switch-to-xbox-360-mode) first.

Turn your controller on. It connects within a few seconds and works like a Pro Controller. By default the buttons follow their positions the way an Xbox controller labels them, so the bottom face button is A. If you'd rather have Nintendo's layout, see [button mapping recipes](button-mapping.md#recipes).

### Sleep and wake

- **The dongle can't wake the Switch.** When the Switch goes to sleep it stops powering the dongle, which turns off with it. Wake the Switch with its power button or the HOME button on a Joy-Con. (Users have reported that a USB-to-Ethernet adapter plugged into the dock keeps its USB ports powered during sleep; this hasn't been tested with the dongle.)
- **After the Switch wakes up**, the dongle starts again. Turn your controller on (on an Xbox controller, press the Xbox button) and it reconnects by itself.
- **Updating the firmware?** Confirm the new firmware in the console before you dock the dongle. Sleep cuts the power, and a firmware update you haven't confirmed is undone the next time the dongle restarts. See [Confirm or roll back](updating.md#confirm-or-roll-back).

### Change Grip/Order

On the **Change Grip/Order** screen (**Controllers > Change Grip/Order** on the HOME Menu), the Switch asks you to press L and R to register a controller. You don't have to do that with the dongle. Every time the Switch finishes connecting to it over USB, the dongle holds L and R for one second by itself, so it registers without a hand on the controller. Because it happens on every new USB connection, such as after you plug the dongle in or wake the Switch, you may see L and R pressed for a moment at those times.

The Switch sometimes drops the dongle on this screen. When that happens, the dongle reconnects itself after about 2.5 seconds. If it stays gone, unplug it and plug it back in.

### Play with more than one player

Each dongle carries one controller. For local multiplayer, use one dongle and one controller per player. Each dongle has its own identity, so the Switch treats them as separate Pro Controllers.

Pair each controller with its own dongle, one dongle at a time. A new dongle is in pairing mode as soon as it gets power, and it takes the first controller it finds (see [Pair a controller](getting-started.md#pair-a-controller)).

## PC, Steam Deck and Android

Most PC games, Steam and Android expect an Xbox controller. In Xbox 360 mode the dongle is a wired Xbox 360 controller, which these systems recognize without any extra software.

### Switch to Xbox 360 mode

You change the mode once, from the web console. Use a computer with desktop Chrome or Edge; a Mac works too.

1. Plug the dongle into the computer. Open the web console at [finger563.github.io/esp-usb-ble-hid/dongle_console.html](https://finger563.github.io/esp-usb-ble-hid/dongle_console.html), click **Connect dongle** and pick **Pro Controller**. For help opening the console, see [Web console](web-console.md).
2. Open the **Settings** tab. Set **USB controller** to **Xbox 360 Controller (X-Input)** and click **Apply changes**.
3. Restart the dongle: on the **Device** tab click **Reboot dongle**, or unplug the dongle and plug it back in. The new mode takes effect when the dongle restarts. Until then, the Device tab's **USB controller** row shows "Switch Pro — set to Xbox 360, reboot to apply".
4. The dongle comes back as an Xbox 360 controller. The browser treats that as a different device, so click **Connect dongle** once more and pick **Xbox 360 Controller**. After that, the console reconnects to either identity by itself.
5. Check the result. The Device tab's **USB controller** row now reads **Xbox 360**. On the T-Dongle-S3's screen, the host icon changes from a Switch to an Xbox logo.

Now plug the dongle into your PC, Steam Deck or Android device. The dongle stores the mode, so it stays an Xbox 360 controller until you change it back.

**To go back to Switch Pro mode**, follow the same steps with the names swapped: connect to **Xbox 360 Controller**, set **USB controller** to **Switch Pro Controller**, and restart the dongle. In a browser that has already connected to both identities, the console reconnects by itself after the restart. Otherwise, click **Connect dongle** once more and pick **Pro Controller**.

<!-- screenshot: Settings tab with the USB controller drop-down set to Xbox 360, and the Device tab's USB controller row showing "reboot to apply" -->

> [!NOTE]
> The button on the dongle only starts pairing. You can change the mode only from the web console. **Reset to defaults** on the Settings tab also sets the mode back to the one the firmware starts in (Switch Pro for the release firmware), from the next restart on (see [Settings](web-console.md#settings)). If you build your own firmware, you can make it start in Xbox 360 mode (see [Configuration options](building.md#configuration-options)).

### What you get in Xbox 360 mode

- **A wired Xbox 360 controller** that games with Xbox controller (X-Input) support recognize.
- **Windows** binds its own built-in Xbox 360 driver, so there is nothing to install.
- **Steam and the Steam Deck** see an Xbox 360 controller. The T-Dongle-S3 has a USB-A plug, so connect it to the Deck through a USB-C adapter or a dock.
- **Linux** uses the kernel's `xpad` driver. To use the web console on Linux, see [Linux permissions](#linux-permissions).
- **Android**: connect through a USB-C (OTG) adapter.
- **Analog triggers** (LT / RT). Your controller's Xbox or Home button becomes **Guide**, and View and Menu become **Back** and **Start**.
- **Rumble** sent by the PC drives the motors of the Xbox Wireless controller (Series X|S, model 1914; the LED part of the report is ignored).
- **Not included:** the Share / Capture button, because X-Input has no such button (you can [remap it](button-mapping.md) to something else). The controller's battery level isn't reported.

You can still use the web console in Xbox 360 mode. On a computer that is also your game PC, the console and the controller work side by side.

## macOS and iOS

- **Use Switch Pro mode.** macOS has no Xbox 360 (X-Input) driver, so in Xbox 360 mode nothing on the Mac reads the controller. The web console still works in that mode. The Device tab's Link block then says "host is not reading X-Input reports", which is expected on a Mac.
- **Game support varies.** In Switch Pro mode the dongle looks like a wired Nintendo Switch Pro Controller. Whether a game on a Mac, iPhone or iPad accepts it depends on the system and the game. This hasn't been tested for this project yet, so if you try it, please [report what happens](troubleshooting.md#reporting-a-bug).
- **A Mac can set up a dongle for a PC.** Change the mode on the Mac, then plug the dongle into the PC. The setting stays on the dongle.
- **iPhone and iPad can't run the web console.** It needs desktop Chrome or Edge, so use a computer for setup and updates. The dongle also needs an adapter for the iPhone's or iPad's port.

## Linux permissions

On Linux, your user needs permission to open the dongle. Without it:

- the web console lists the dongle but can't open it, and
- the programmer and the console's [Full update](updating.md#full-update) can't open the serial port the dongle has in download mode.

These are the USB IDs the dongle uses:

| When | USB ID (vendor:product) | Used by |
|---|---|---|
| Switch Pro mode | `057e:2009` | Web console (WebUSB) |
| Xbox 360 mode | `045e:0e60` | Web console (WebUSB) |
| Download mode (usually) | `303a:1001` | Programmer, Full update (Web Serial) |
| Download mode (on some boards) | `303a:0009` | Programmer, Full update (Web Serial) |

To grant access, add a udev rule file:

```sh
sudo tee /etc/udev/rules.d/60-esp-usb-ble-hid.rules > /dev/null <<'EOF'
# esp-usb-ble-hid dongle: web console over WebUSB (Switch Pro and Xbox 360 modes)
SUBSYSTEM=="usb", ATTR{idVendor}=="057e", ATTR{idProduct}=="2009", MODE="0660", TAG+="uaccess"
SUBSYSTEM=="usb", ATTR{idVendor}=="045e", ATTR{idProduct}=="0e60", MODE="0660", TAG+="uaccess"
# ESP32-S3 download mode: the programmer and the console's Full update
SUBSYSTEM=="tty", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="1001", MODE="0660", TAG+="uaccess"
SUBSYSTEM=="tty", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="0009", MODE="0660", TAG+="uaccess"
EOF
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Then unplug the dongle and plug it back in.

- `TAG+="uaccess"` gives the logged-in user access on systems that use systemd-logind, which most desktop distributions do. On other systems, use `MODE="0666"` instead.
- For the serial ports alone, another option is to add yourself to the group that owns them and then log out and back in: `sudo usermod -aG dialout $USER` (the group is `uucp` on Arch Linux).
- If you built your own firmware with a different Xbox 360 product ID, put that ID in the second rule.
- If your browser is a Snap or Flatpak, its sandbox can also hide USB devices. A regular package of Chrome or Edge avoids that.
- On a Steam Deck, it is usually easier to change the dongle's settings from another computer. The settings are stored on the dongle.
