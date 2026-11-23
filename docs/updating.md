# Updating the firmware

This page covers every way to update the dongle's firmware. Everyday updates take one click in the [web console](web-console.md). Moving from v3 to v4 takes a one-time Full update from the browser. Developers can also update from the command line.

> [!WARNING]
> **Release firmware is built for the LilyGo T-Dongle-S3 only**, and the console offers it to every dongle without checking which board it is. If the Device tab's **Hardware** row shows anything other than *LilyGo T-Dongle-S3* (for example *Adafruit QT Py ESP32-S3* or *Generic ESP32-S3*), don't use **Download and install** or the Full update's published images. Build firmware for your board instead (see [Build for your board](building.md#build-for-your-board)), then install it with [your own .bin](#install-your-own-bin) or [the command line](#update-from-the-command-line). Those two replace only the app, so to move such a board from v3 to v4, flash your whole build over serial as the building guide describes, or run the Full update with [your build's image files](#from-local-image-files). If you installed a release image by mistake with **Download and install**, don't confirm it: unplug the dongle and plug it back in, and it returns to the previous firmware.

## Which update do you need

| Your dongle runs | What to do |
|---|---|
| v4.0.0 or later | One click: [update from the console](#update-from-the-console) |
| v3.x | One [Full update](#full-update) to move to v4 (the console offers it). Then use one-click updates. On v3.0.0, see [If the dongle can't restart itself](#if-the-dongle-cant-restart-itself). |
| v2.1.0 or older | One [Full update](#full-update) using the BOOT button, because these versions have no web console. Or use [the programmer](#re-run-the-programmer). |
| Your own build, or a board other than the T-Dongle-S3 | [Install your own .bin](#install-your-own-bin) or [update from the command line](#update-from-the-command-line). To move from v3 to v4, run the Full update with [your build's image files](#from-local-image-files) or flash the whole build over serial ([Build for your board](building.md#build-for-your-board)). |

To find out which version you have, connect the dongle in the [web console](web-console.md#connect-the-dongle). The version appears in the header and on the Device tab's **Firmware** row. If the browser lists the dongle but the console can't open it, and the log says it has no vendor-specific interface, the dongle is probably on v2.1.0 or older.

Updates run from a computer, so unplug the dongle from the Switch (or wherever it lives) and plug it into a computer first.

## Update from the console

1. [Open the web console](web-console.md#open-the-console) and connect the dongle.
2. Go to the **Firmware** tab. The console has already checked GitHub for the latest release. The **Update firmware** card says *Up to date*, or *Update available* with the version, its release date and a link to the release notes. The Device tab's **Latest release** row shows the same thing.
3. Click **Download and install vX.Y.Z**. The file name and size are shown next to the button. Confirm.
4. Wait. The dongle first erases the slot the new firmware goes into, which takes a few seconds. Then the progress bar fills. At the end, the dongle checks the image and restarts into it. The USB link drops, which is expected.
5. The console reconnects by itself (if [Auto-connect](web-console.md#auto-connect) is on) and opens the Firmware tab. The new firmware is marked *pending verify*. [Confirm it](#confirm-or-roll-back) once you have checked that it works.

<!-- screenshot: the Firmware tab's Update firmware card showing "Update available" and the Download and install button -->

Good to know:

- **Only the firmware itself is replaced.** Settings, paired controllers and button maps stay as they are.
- **The download comes from GitHub Pages, not GitHub Releases.** A web page isn't allowed to download GitHub release files directly, because they are served without CORS headers. So each release's images are mirrored next to the console, and the console downloads from there. Right after a new release is published, the mirror can take a little while to catch up. If the download fails, the console shows a direct link to the file: save it and [install it yourself](#install-your-own-bin).
- **An interrupted update is harmless.** The new image goes into the dongle's other firmware slot (it has two), and the dongle only switches to it after the image has been checked. Until then it keeps running the old firmware. **Abort update** stops after the chunk in flight and throws the partial image away.
- **The update check is cached.** The console remembers the answer for 10 minutes. **Check for updates** asks again. If GitHub can't be reached or limits the request, the console says so and keeps working.
- **Reinstalling works too.** **Download and install** is also offered when you already run the latest release, and the confirmation says so.
- **Reboot after changing the USB controller type, before you update.** Otherwise the update's reboot also switches the mode. The console copes with that, but you may need to click **Connect dongle** once to allow the dongle's other identity (see [Auto-connect](web-console.md#auto-connect)).

### Install your own .bin

Use this for a build of your own, for a board other than the T-Dongle-S3, or for a release file you downloaded by hand.

1. On the **Firmware** tab, click **Choose .bin file** and pick the app image. For your own build, that is `build/esp-usb-ble-hid.bin`. For a release, it is the `esp-usb-ble-hid.bin` asset. Don't pick `bootloader.bin`, `partition-table.bin` or `ota_data_initial.bin`.
2. Click **Flash firmware**.
3. Confirm the new image when the dongle is back, as described below.

This goes through the same steps and safety checks as **Download and install**. The console warns you if the file doesn't look like a firmware image, and the dongle rejects such a file at the first chunk. You can usually leave **Chunk size** (default 1024 B) as it is. This route replaces only the app: to change the partition table, use the [Full update](#full-update).

## Confirm or roll back

A new image starts out on probation. After an update, the dongle boots the new firmware in a *pending verify* state. Until you confirm it, the firmware you had before is kept as a fallback.

On the **Firmware** tab, the **Running image** card shows *PENDING VERIFY* and a panel with two buttons:

- **Confirm this firmware**: keep it. Do this once you have checked that it works. For example, the controller connects (Device tab) and its buttons show up in **Button mapping**'s live view.
- **Roll back to previous**: go back to the old firmware now. The dongle reboots into it and the console reconnects.

<!-- screenshot: the Running image card with the PENDING VERIFY panel and its two buttons -->

> [!WARNING]
> **If you don't confirm, the next reset rolls the dongle back.** A reset includes unplugging the dongle, rebooting it, or the Switch cutting USB power when it goes to sleep. Confirm the new firmware *before* you unplug the dongle or put it back in a dock.

The firmware never confirms itself. This is on purpose: a broken build could otherwise mark itself as good just before it crashes. The console (or the command-line tool) has to confirm it from outside.

When the running image isn't pending verify, the card shows **Mark running image valid** and **Roll back and reboot** instead, which do the same two jobs. **Roll back and reboot** asks you to confirm, then switches to the image in the other slot, if that slot holds a working one. **Check status** reads the state again.

## Full update

A Full update rewrites the dongle's whole firmware from the browser, including the parts an ordinary update can't change. The dongle needs it once to move from v3 to v4. You can also use it any time to repair a dongle.

### When you need it

- **Moving from v3.x to v4.** v4 stores button maps in a new flash partition: it split the old 256 KB crash-dump area into 128 KB for crash dumps and 128 KB for the button maps. An ordinary update only replaces the app, never the partition table (the map of what is stored where in the dongle's flash), so v4 needs one Full update. Your v3 stick and button settings (invert Y, swap A/B, swap X/Y, deadzone) become the default [button map](button-mapping.md) on the first v4 boot.
- **v4 installed over USB on a v3 dongle.** It runs, but it has nowhere to store button maps, so your edits are lost at the next reboot. The Button mapping tab says *Maps are not saved yet*. One Full update fixes that.
- **v2.1.0 or older.** These dongles have an even older single-app layout and no web console. Use the [BOOT-button route](#if-the-dongle-cant-restart-itself) below, or [the programmer](#re-run-the-programmer).

The console tells you when a Full update is needed. A **One-time Full update needed** banner appears on the Device tab, and the Firmware tab's Full update card has a **Needed** row.

### What it writes and what it keeps

It writes the latest release's four images at their places in flash:

| Image | Flash offset |
|---|---|
| Bootloader (`bootloader.bin`) | `0x0` |
| Partition table (`partition-table.bin`) | `0x8000` |
| OTA data (`ota_data_initial.bin`) | `0xf000` |
| The firmware (`esp-usb-ble-hid.bin`) | `0x20000` |

The chip is **not** erased. The storage area that holds your paired controllers and settings (NVS, at `0x9000`) is left alone, and the console refuses any image that would overwrite it. On a dongle that already has the v4 layout, the button maps are kept too.

It uses [esptool-js](https://github.com/espressif/esptool-js) over Web Serial and talks to the ESP32-S3's ROM download mode. That mode is built into the chip and can't be overwritten, so a Full update that fails or is interrupted can always be run again.

### Run it

You need **Chrome or Edge on a desktop** (for Web Serial) and the [hosted console](web-console.md#open-the-console), which fetches the images from next to the page. On Linux, the serial port may need a udev rule too (see [Linux permissions](hosts.md#linux-permissions)).

1. Connect the dongle in the console. Click **Open Full update** on the Device tab's banner, or go to the **Firmware** tab's **Full update (Web Serial)** card.
2. Click **Full update**. The console downloads the published release (the **Published** row shows its version) and checks the images. Then it asks you to confirm, and it asks the dongle to restart into download mode. The USB link drops, which is expected.
3. Click **Select the bootloader port and flash**. A second click is needed because the browser only opens its port picker in response to a click. Pick the Espressif port, either `303a:1001` (*USB JTAG/serial debug unit*) or `303a:0009`. The picker only lists Espressif devices. If the port isn't there yet, close the picker, wait a second and click again.
4. Each image is written, then checked against what is in flash. After that, the dongle restarts into the new firmware. With Auto-connect on, the console opens it again within a few seconds. If it doesn't come back, unplug it and plug it back in.

<!-- screenshot: the Full update card during step 2, with the progress bar and the esptool-js log -->

After a Full update, check the Firmware tab's **Running image** card. If it shows *pending verify*, [confirm it](#confirm-or-roll-back) as usual.

### If the dongle can't restart itself

Step 2 needs the dongle's firmware to restart into download mode. The dongle can't do that if it runs v2.1.0 or older (no console), or v3.0.0 (no System module). The same applies if it doesn't answer at all. In those cases, put it into download mode by hand:

1. Unplug the dongle. Hold its **BOOT** button while you plug it back in, then let go. See [Download mode](hardware.md#download-mode) for where the button is.
2. Open the hosted console's **Firmware** tab. You don't need to connect.
3. Click **Select the bootloader port and flash** and carry on from step 3 above.

On v3.0.0 there is another way if the button is hard to reach. Install v4 with **Download and install** first, and confirm it. It runs on the old layout but can't save button maps yet. Then run the Full update as above: v4 can restart itself into download mode.

### If something goes wrong

- The dongle stays in download mode until it is reset or unplugged. You can simply click **Select the bootloader port and flash** again.
- The log under the card shows what esptool-js did. If it can't reach the chip, it gives up after about 25 seconds.
- **An error about connecting, syncing or a timeout:** make sure you picked the dongle's Espressif port, and that the dongle is in download mode. If in doubt, use the BOOT-button route above.
- **An MD5 or verify error:** the written data didn't match. Try again, ideally with another USB port or cable.

### From local image files

When the page has no published firmware next to it, the card offers one file picker per flash offset, already filled in with `0x0`, `0x8000`, `0xf000` and `0x20000`. This happens with a copy served from `http://localhost`, or before the first release. Pick `build/bootloader/bootloader.bin`, `build/partition_table/partition-table.bin`, `build/ota_data_initial.bin` and `build/esp-usb-ble-hid.bin` from a build or a release. The same checks apply. A copy opened straight from disk (`file://`) can't load the flasher at all, so serve the `web/` folder over `http://localhost` instead (see [Use a local copy](web-console.md#use-a-local-copy)).

## Re-run the programmer

The programmer attached to each [GitHub release](https://github.com/finger563/esp-usb-ble-hid/releases/latest) writes the same four images as the Full update and doesn't erase the chip, so paired controllers and settings survive. It is the fallback when the browser route doesn't work for you. Like the release images, it is for the **T-Dongle-S3** only.

1. Put the dongle into download mode. Either hold BOOT while you plug it in, or use the console's **System** tab, **Reboot into bootloader**. See [Download mode](hardware.md#download-mode).
2. Run the programmer for your system. [Install the firmware](getting-started.md#install-the-firmware) lists the files and how to run each.
3. If the dongle doesn't restart into the new firmware by itself when the programmer finishes, unplug it and plug it back in.

## Update from the command line

These options are for developers who build the firmware themselves. Both tools update the dongle over the same USB cable it already uses, with no serial port and no BOOT button. The dongle must already run v3.0 or later, with the console interface enabled (it is by default).

Both tools find the dongle by its USB identity, and their default identity is espp's (`0x1209:0x0d32`), not the dongle's. So always say which identity to look for:

| Dongle mode | Vendor ID | Product ID |
|---|---|---|
| Switch Pro | `0x057E` | `0x2009` |
| Xbox 360 | `0x045E` | `0x0E60` (or your `CONFIG_DONGLE_USB_XBOX_PID`) |

Both need [pyusb](https://pypi.org/project/pyusb/) with a libusb backend: `pip install pyusb` (for `idf.py ota-usb`, into the ESP-IDF Python environment), plus `brew install libusb` on macOS or `apt install libusb-1.0-0` on Linux. On Linux they also need the udev rule from [Linux permissions](hosts.md#linux-permissions).

### idf.py ota-usb

In the project folder, with the ESP-IDF environment loaded:

```sh
# Switch Pro mode
ESPP_OTA_VID=0x057E ESPP_OTA_PID=0x2009 idf.py ota-usb

# Xbox 360 mode
ESPP_OTA_VID=0x045E ESPP_OTA_PID=0x0E60 idf.py ota-usb
```

It builds the firmware first, so you don't need a separate `idf.py build`. Then it sends the image over USB, waits for the dongle to reboot, reconnects, and confirms the new image once it answers. With several dongles plugged in, add `ESPP_OTA_SERIAL=<serial>`. That is the dongle's USB serial number, which the web console's log prints when it connects.

This project gets espp from the ESP Component Registry. ESP-IDF 6.0.2 and later only load `idf.py` extensions from trusted sources, so out of the box `ota-usb` is a plain build target that takes no options. That is why the device is chosen through environment variables. To get the full action with options such as `--vid`, `--pid`, `--no-verify`, `--status` and `--mark-valid`, set `IDF_EXTENSION_ALLOW_UNTRUSTED=1` or install the espp Python package in the ESP-IDF environment. espp's [OTA tool README](https://github.com/esp-cpp/espp/tree/main/components/ota/python) has the details. The [building guide](building.md) covers the rest of the development loop.

### espp-ota

The same tool on its own, for an image you already have. `pip install "espp[usb]"` provides the `espp-ota` command:

```sh
export ESPP_OTA_VID=0x057E ESPP_OTA_PID=0x2009   # or 0x045E / 0x0E60 in Xbox 360 mode

espp-ota flash build/esp-usb-ble-hid.bin   # update, reconnect, confirm
espp-ota status                            # is the running image pending verify?
espp-ota mark-valid                        # confirm the running image
espp-ota rollback                          # reject it: roll back and reboot
espp-ota list                              # list matching USB devices
```

Each command also takes `--vid`, `--pid` and `--serial`. After `flash` reconnects, it confirms the new image for you. Pass `--no-verify` to skip that, then either confirm later with `mark-valid` or let the image roll back.

> [!NOTE]
> If the USB controller type was changed but the dongle hasn't rebooted since, the update's reboot also switches the mode. The dongle then comes back under its other identity. The tool looks for the identity it flashed, reports the dongle as missing, and leaves the new image unconfirmed. Before the next reset, confirm it with the other identity (for example `espp-ota mark-valid --vid 0x045E --pid 0x0E60`) or from the web console, which finds the dongle in either mode. Better still, reboot after changing the type and before updating.
