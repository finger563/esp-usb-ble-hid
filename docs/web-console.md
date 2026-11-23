# Web console

The web console is a page you open in your browser that talks to the dongle over USB. Use it to check the link to your controller, manage paired controllers, change settings, remap buttons, update the firmware and collect crash reports, with nothing to install.

<!-- screenshot: the v4 console's Device tab, connected, with a controller paired: Status card, Link block, Paired controllers and Actions -->

## What you need

- **A desktop computer with Chrome, Edge, Brave or another Chromium-based browser.** The console uses WebUSB, the browser feature that lets a web page talk to a USB device. Firefox and Safari don't implement it.
- **For the [Full update](updating.md#full-update) only: Chrome or Edge on a desktop.** It also needs Web Serial, the browser's access to serial ports.
- **A USB connection from the dongle to that computer.** The console runs in the computer's browser, so a dongle that normally sits in a Switch dock has to be unplugged and brought to the computer. Most of the time you only need the console to set things up or update the firmware.
- **On Linux, permission to open the device.** You need a udev rule. See [Linux permissions](hosts.md#linux-permissions).

On Windows and macOS you don't need a driver. The dongle's console interface tells Windows to use its built-in WinUSB driver.

## Open the console

Plug the dongle into the computer and open:

**<https://finger563.github.io/esp-usb-ble-hid/dongle_console.html>**

Bookmark it. This hosted copy is the one to use: it can install firmware updates, and it also works with dongles running older firmware (see [Older firmware](#older-firmware)).

### Use a local copy

The console is a single file, [`web/dongle_console.html`](../web/dongle_console.html), plus `web/esptool-bundle.js` for the Full update. You can run your own copy, for example offline or while developing, but some features need the hosted page:

| How you open it | Connect | Download and install | Full update |
|---|---|---|---|
| The hosted page (`https://…`) | Yes, with Auto-connect | Yes | Yes, with the published release |
| A copy served from `http://localhost` (for example `python3 -m http.server` in `web/`, then `http://localhost:8000/dongle_console.html`) | Yes, with Auto-connect | No: offers a direct download link instead | Only with image files you pick yourself |
| The file opened straight from disk (`file://`) | **Connect dongle** works, but the browser may not remember the permission after a reload, so don't rely on Auto-connect | No: offers a direct download link instead | No: the flasher can't be loaded from a file |

WebUSB only works on pages the browser treats as secure. The hosted page (HTTPS) and `http://localhost` both qualify, and Chromium browsers also allow a file opened from disk, with the limits shown in the table.

## Connect the dongle

1. Click **Connect dongle** at the top of the page.
2. The browser lists the matching USB devices. The dongle is called **Pro Controller** in Switch Pro mode and **Xbox 360 Controller** in Xbox 360 mode. Pick it and click **Connect**. If a real Pro Controller is also plugged in by USB, it shows up under the same name.
3. The header changes to **Connected** and shows the dongle's name and firmware version. Each tab then fills in.

The same button then reads **Disconnect**. Click it to release the dongle, for example so that another app or browser tab can use it.

**Pick any USB device…** lists every USB device instead of just the dongle's two identities. You only need it for a build that uses its own USB product ID.

If the browser lists the dongle but the console can't open it, another tab or app may be holding it: close the other one and try again. If the log says the device has *no vendor-specific interface*, the dongle has no console. It is either running v2.1.0 or older (see [Full update](updating.md#full-update)), or it was built without the console interface.

The console has six tabs: **Device**, **Settings**, **Button mapping**, **System**, **Firmware** and **Crash dump**, each described below. You can move between them with the arrow keys, Home and End as well as the mouse, and the console reopens the last tab you used.

## Auto-connect

The browser remembers which dongles you have allowed this page to use. After the first **Connect dongle**, the console opens an allowed dongle by itself, with no device picker, in three cases:

- **When the page loads**, if exactly one allowed dongle is plugged in. If there are several, it doesn't guess: pick one with **Connect dongle**.
- **When you plug in an allowed dongle** while the console isn't connected to anything.
- **When the link drops.** After a reboot the console asked for (the Device tab's **Reboot dongle**, the System tab's **Reboot**, a firmware update or a rollback), it waits for the same dongle to come back and reconnects, for about 90 seconds. After a drop it didn't ask for, such as an unplug or a crash, it tries for about 30 seconds. Either way it tells dongles apart by their USB serial number.

After **Reboot into bootloader** (System tab), the console doesn't follow the dongle into download mode, because there it is a different USB device. Once it has been flashed and reset, it comes back as a dongle and is opened like any plug-in. The same happens after a [Full update](updating.md#full-update).

> [!NOTE]
> Changing the USB controller type changes how the dongle identifies itself over USB. The browser treats the new identity as a different device. The first time the dongle comes back in the other mode, click **Connect dongle** once to allow it. The console's log says when this is needed. After that, both identities are remembered.

### Turn it off

Untick the **Auto-connect** checkbox next to **Connect dongle**. It is on by default, and the browser remembers your choice. With it off, the console only connects when you click **Connect dongle**. Clicking **Connect dongle** always works, either way.

### Link straight to one dongle

A link like this opens one particular dongle when the page loads:

```
https://finger563.github.io/esp-usb-ble-hid/dongle_console.html?autoconnect=1&vid=0x057e&pid=0x2009&serial=<serial>
```

- `vid` and `pid` are required and must be one of the dongle's identities: `0x057e` / `0x2009` (Switch Pro mode) or `0x045e` / `0x0e60` (Xbox 360 mode). Hex (`0x…`) or decimal both work.
- `serial` is optional. It is the dongle's USB serial number, which the console's log prints when it connects. With it, only that dongle is opened.
- The link only opens a dongle this page has already been allowed to use, and only while Auto-connect is on.

espp's Device Hub (one of the [espp web apps](https://esp-cpp.github.io/espp/apps/index.html)) hands devices to an app's page with links of this form.

## Device

The Device tab is the dongle's dashboard. If the dongle needs the one-time Full update, a **One-time Full update needed** banner at the top explains why and opens it.

### Status

<!-- screenshot: the Status card with its badges and rows, a controller connected -->

The badges show whether USB is mounted, whether a controller is connected, and whether the dongle is scanning or in pairing mode. The rows below them are:

- **USB controller**: the mode the dongle is running in (Switch Pro or Xbox 360). If you changed it in Settings, this row also says *set to …, reboot to apply*.
- **Controller** and **Battery**: the connected controller's serial number and battery level.
- **Paired controllers**: how many controllers the dongle remembers.
- **Uptime**, **Project**, **Firmware**, **Hardware** and **ESP-IDF**: what is running, and on which board.
- **Latest release**: whether a newer firmware exists. See [Updating the firmware](updating.md).

The card refreshes every 2 seconds while the page is visible. **Refresh now** reads it straight away.

### Link

The Link block is the first place to look when a controller is connected but nothing happens:

- **Controller link** shows where the Bluetooth connection is: *scanning*, *connecting*, *encrypting*, *subscribing*, then *subscribed* once inputs flow. The dongle adds a note about the last thing that happened, such as why a link dropped or what it is retrying.
- **BLE input** counts the input messages the controller has sent and shows how long ago the last one arrived.
- **USB output** shows the host side. In Switch Pro mode, it shows whether the host has finished the Pro Controller handshake. In Xbox 360 mode, it shows whether a driver is reading the X-Input reports. Either way it also shows how many reports were sent.

A warning appears under the block when something looks stuck. For example, the controller has sent nothing for more than 5 seconds, the link is stuck while being set up, or the host isn't taking reports. For what to do next, see [Troubleshooting](troubleshooting.md).

### Paired controllers

Every controller the dongle remembers is listed by the name it reported when it last connected, followed by its Bluetooth address. A controller shows as *Unknown controller* until the dongle has learned its name. The one in use has a *connected* badge.

**Forget** removes one controller, after you confirm. It has to be paired again before it can reconnect. If you forget the last one, the dongle goes into pairing mode by itself.

### Actions

- **Start pairing**: the same as holding the dongle's button. See [Pair a controller](getting-started.md#pair-a-controller).
- **Forget all controllers**: clears every pairing, after you confirm. The dongle then goes into pairing mode.
- **Reboot dongle**: restarts the dongle, after you confirm. The USB link drops for a few seconds and the console reconnects (see [Auto-connect](#auto-connect)).

## Settings

<!-- screenshot: the v4 Settings tab, with the USB controller select and the LED sliders -->

The dongle stores its settings in flash. They are kept across firmware updates, including the Full update. Change what you want, then click **Apply changes**. Only the settings you changed are sent, and the tab shows how many changes are unsaved. **Reload** reads the settings from the dongle again.

| Setting | Default | Range | Takes effect |
|---|---|---|---|
| **USB controller** | Switch Pro Controller | Switch Pro Controller, or Xbox 360 Controller (X-Input) | After a reboot. See [Switch to Xbox 360 mode](hosts.md#switch-to-xbox-360-mode). |
| **Status LED brightness** | 100 % | 0–100 % | Straight away |
| **LED level while connected** | 25 % | 0–100 % | Straight away |
| **LED level while reconnecting** | 1 % | 0–100 % | Straight away |
| **LED level while pairing** | 100 % | 0–100 % | Straight away |
| **Blink LED on every input** | Off | On / off | Straight away |
| **BLE device name** | `Switch` | 1–31 printable ASCII characters | After a reboot |

- **USB controller** decides what the dongle is to the USB host. Use **Switch Pro Controller** for the Switch and anything that accepts a Pro Controller, and **Xbox 360 Controller** for Windows, Linux, Steam and Android. The Switch and macOS can't use Xbox 360 mode. See [What you can plug it into](hosts.md) for details. A dongle built with `CONFIG_DONGLE_DEFAULT_XBOX360`, which includes the [generic ESP32-S3 build](building.md#build-for-your-board), starts in Xbox 360 mode on its first boot.
- **Status LED brightness** is the overall brightness. The three **LED level** settings are shares of it: the steady light while a controller is connected, and the peak of the slow breathing while the dongle looks for a paired controller or the fast breathing while it pairs. Reconnecting is set very dim by default, because the dongle can spend a long time looking for a controller. Set a level to 0 to turn the LED off in that state. [LED and button](hardware.md#led-and-button) lists what each LED pattern means.
- **Blink LED on every input** is a debugging aid. The LED toggles with every controller input the dongle forwards to the host, instead of staying steady.
- **BLE device name** is the name the dongle uses for itself over Bluetooth LE. Most people never need to change it.

The stick and button options from older firmware (invert Y, swap A/B, swap X/Y, stick deadzone) are now part of **Button mapping**. With v4 firmware, the Settings tab hides them and links there.

### Reset to defaults

**Reset to defaults** restores every setting in the table above to the values the firmware starts with, after you confirm. That includes the USB controller type: Switch Pro for the release firmware, Xbox 360 for a build made to start in Xbox 360 mode. As with **Apply changes**, the LED settings change straight away, and the USB controller type and BLE device name at the next reboot. Paired controllers and button maps are not touched. The Device tab's USB controller row shows when a mode change is waiting for a reboot.

> [!NOTE]
> Firmware v4.0.0 always reset the USB controller to Switch Pro, even on a build that starts in Xbox 360 mode.

## Button mapping

This tab edits how the controller's buttons and sticks reach the host. It has a live view of the controller's input before and after mapping (which also makes it a handy input tester), a default map, one profile per controller model, presets and **Learn**. It needs firmware v4. Older firmware shows a note that links to the Full update instead.

See the [button mapping guide](button-mapping.md) for how it works and for [recipes](button-mapping.md#recipes).

## System

<!-- screenshot: the System tab with the info rows and the Memory gauges -->

The **System** card shows what the dongle is and what state it is in: chip and revision, CPU speed, ESP-IDF and firmware versions, build date, the running and boot partitions with their update state, the reason for the last reset, uptime, MAC address, flash and PSRAM size, and free heap. **Refresh** reads it again.

The **Memory** card shows a gauge for each memory region: how much is in use, plus a red mark at the highest use since boot. You only need it when chasing a bug.

There are two buttons, each with a confirmation step. The **Delay** field next to them sets how long the dongle waits before it restarts (250 ms by default; a reboot into the bootloader always waits at least 1 second):

- **Reboot** restarts the dongle. The console reconnects as it does after **Reboot dongle**.
- **Reboot into bootloader** restarts the dongle in the ESP32-S3's ROM download mode, the mode used to write new firmware over USB. You don't need to press the BOOT button, which helps when it is hard to reach. The dongle stays in download mode until it is flashed and reset, or unplugged and plugged back in. The console doesn't reconnect until then. See [Download mode](hardware.md#download-mode). The Firmware tab's Full update uses this step for you.

## Firmware

The Firmware tab installs updates. **Running image** shows the firmware and asks you to confirm a freshly installed one. **Update firmware** checks GitHub for the latest release, installs it with one click, or installs a `.bin` you choose. **Full update (Web Serial)** writes the complete firmware, partition table included, for the one-time move to v4.

<!-- screenshot: the Firmware tab right after an update: the Pending verify panel with Confirm this firmware / Roll back to previous -->

All of this is explained in [Updating the firmware](updating.md), including [confirming or rolling back](updating.md#confirm-or-roll-back) a new image.

## Crash dump

If the firmware ever crashes, the dongle keeps a record of the crash (a core dump) in flash. This tab reads it:

- **Last crash** shows a short summary, or *clean boot history* when there is nothing to report, plus the size of the stored dump. **Refresh** reads it again.
- **Download core.elf** saves the dump as `core.elf`. This is the file to attach to a bug report.
- **Download raw image** saves the whole stored blob as `coredump_raw.bin`.
- **Erase stored dump** clears it, after you confirm.

**Decode on your PC** shows the `espcoredump.py` command, with a **Copy** button, for turning the dump into a readable backtrace. It needs the matching firmware ELF: for a release, that is the `esp-usb-ble-hid.elf` attached to the same GitHub release. See [Reporting a bug](troubleshooting.md#reporting-a-bug).

## Log panel

The panel at the bottom of the page lists what the console did and what the dongle answered, with timestamps. Tick **Log frames (hex)** to also print every message exchanged with the dongle. **Clear log** empties the panel. When something goes wrong, copy the log into your bug report.

## Older firmware

The hosted console works with older dongles:

- **A tab whose feature the firmware doesn't have is disabled.** Hover over it to see why. There are two exceptions. **Button mapping** stays open to explain that it needs v4. **Firmware** stays open because its Full update works without anything on the dongle's side.
- **A setting the firmware doesn't know is dimmed and read-only**, marked *Not supported by this firmware*.
- **v3 firmware** still applies the old stick and button options, so the Settings tab shows them for v3 dongles. The console also offers the one-time [Full update](updating.md#full-update) to move to v4.
- **v2.1.0 and older** have no console. The browser lists the dongle but can't open it. Use the [Full update](updating.md#full-update) with the BOOT button, or the programmer.

## What the page sends over the network

The console talks to the dongle over USB. It contacts the internet only for the following:

- **The update check.** After you connect, and when you click **Check for updates**, it asks GitHub's public API for the latest release of this project. The request carries nothing about your dongle: the comparison happens in your browser. The answer is remembered for 10 minutes.
- **Firmware images**, which come from the same site as the console (`firmware/latest/` next to the page). If that copy is missing or is of another release, **Download and install** tries GitHub's own download link instead. The console reads the list of published images when you open the Firmware tab, or when it shows the Full update banner. It only downloads the images themselves when you click **Download and install** or run a Full update.
- **The flasher** (`esptool-bundle.js`), which is loaded from next to the page, and only for a Full update.

Everything else works offline. The browser also stores a few preferences on your computer: the last tab you opened, the Auto-connect choice and the cached update check.

## Protocol details

The console talks to the firmware through a set of modules on one USB stream. Each module has its own message format. For the module list, message formats, the Full update's published layout, the console's tests and the bundled esptool-js, see the [console protocol notes](../web/README.md). For how the firmware side is put together, see [Architecture](architecture.md).
