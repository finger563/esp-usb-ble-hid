# Dongle Console

`dongle_console.html` is a single-file browser console for the ESP USB BLE HID
dongle. It talks to the dongle over **WebUSB** (the vendor / WebUSB interface the
firmware exposes next to its gamepad HID interface) using espp `stream_frame`
framing and the espp `dispatcher` modules the firmware registers.

Hosted copy: <https://finger563.github.io/esp-usb-ble-hid/dongle_console.html>

## Opening it

- Use a Chromium-based browser (Chrome, Edge, Brave, Opera …). Firefox and Safari
  do not implement WebUSB.
- WebUSB only works from a secure context: `https://` (the hosted copy above),
  `http://localhost`, or a local `file://` path. Double-clicking the file works.
- No build step, no CDN: the page is self-contained (inline CSS + JS), except
  for the Full update's flasher, `esptool-bundle.js`, which sits next to it and
  is imported from the same origin (see [esptool-js](#esptool-js-attribution)).
  Copy both files anywhere. Its only network access is the optional
  firmware-update check against GitHub Releases and the Full update's
  `firmware/latest/` files next to the page (see the Firmware tab below);
  everything else works offline.
- Click **Connect dongle** and pick the device. The dongle enumerates as a
  *Nintendo Switch Pro Controller* (VID `057E`, PID `2009`); **Pick any USB
  device…** lists every device if you are on a modified build.
- On Linux you may need a udev rule granting your user access to `057e:2009`.

On connect the console sends a dispatcher discovery request and shows the
device name / firmware in the header. A tab whose module the firmware does not
advertise is disabled, except **Button mapping** (it explains that the firmware
predates v4) and **Firmware** (its Full update needs no module).

When the connected dongle needs the one-time [Full update](#full-update-web-serial)
— it runs firmware older than v4 (discovery lists no `espp.gamepad_map`
module), or a v4+ image on the old partition table (device-config INFO flag
bit5, `needs_full_flash`) — a banner at the top of the Device tab says so and
links to it.

## Tabs

### Device (module `0x10`, `device_config`)

- **Status** — USB mounted / controller connected / scanning / pairing badges,
  the connected controller's serial and battery, number of paired controllers,
  uptime, project, firmware, hardware and ESP-IDF version. Auto-refreshes every
  2 s while connected (pauses while another transfer is running).
  The **Link** block underneath answers "controller connected but no inputs?":
  *Controller link* shows which step of the bring-up the dongle is in
  (scanning → connecting → encrypting → subscribing → subscribed) and the
  dongle's own note on the last link event (why a link was dropped, what it is
  retrying); *BLE input* shows the HID notifications received since the controller
  subscribed and how long ago the last one arrived; *USB output* shows whether
  the host (the Switch) has finished the Pro Controller handshake and how many
  input reports it has accepted. A warning appears when a connected controller
  has been silent for more than 5 s, or when USB is mounted but the host has
  not enabled input reports yet. Firmware without the INFO link trailer shows
  "—" / no counters.
- **Paired controllers** — one row per BLE bond, name first: the name the
  controller reported the last time it connected (e.g. "Xbox Wireless
  Controller"; "Unknown controller" until the dongle has learned it), then its
  MAC address, address type (public / random) and a *connected* badge for the
  controller currently in use. **Forget** on a row drops that bond only
  (in-page confirmation). The list refreshes after connecting, after any
  action and quietly alongside the 2 s status poll.
- **Actions** — *Start pairing* (same as holding the dongle's button), *Test
  rumble* (`ACTION TEST_RUMBLE`: the test pattern, strong motor then weak, on
  the connected controller through its saved rumble options; the device's
  ERROR message says why when it cannot, e.g. rumble is off for that
  controller), *Forget all controllers* (clears every BLE bond, with an in-page
  confirmation),
  *Reboot dongle* (the console reconnects automatically when the dongle
  re-enumerates).

### Settings (module `0x10`, `device_config`)

USB controller type (Switch Pro / Xbox 360, applies after a reboot),
status-LED brightness, the LED levels while connected / reconnecting / pairing
(shares of that brightness; 0 = off), an optional blink-on-every-input
debugging mode (off by default), BLE device name (applies after a reboot).
**Apply changes** sends only the keys you changed (`SET_SETTINGS` is partial);
the form always re-renders from the dongle's reply. **Reset to defaults**
restores the firmware defaults.

The stick / button options (invert left / right stick Y, swap A/B, swap X/Y,
stick deadzone) moved to the **Button mapping** tab in firmware v4, which
ignores those keys (it migrated them into the default map once): against v4
the form hides them, never sends them, and says "Stick / button options moved
to Button mapping". Against v3 firmware (no `espp.gamepad_map` module) they are
shown as before, since v3 still applies them.

### Button mapping (module `11`, espp `GamepadMapService`, firmware v4+)

The dongle decodes any BLE gamepad into one controller state (buttons named by
position: South = Xbox A / Nintendo B, ...) and rewrites it with a **map**
before sending it to the USB host: the **default map** for every controller,
or a **profile** for one controller type (its Bluetooth VID:PID; by name when
both are 0). The tab speaks `espp.gamepad_map` v1 (resolved through discovery
by protocol id, default module 11) on the console's WebUSB link:

- **Live input** — the controller's input before (raw) and after (mapped) the
  map, buttons, sticks (with the editor's dead zone drawn on the raw sticks)
  and triggers. The dongle streams it while this tab is open and visible with
  *Live input* on: the console renews the stream every 3 s and switches it off
  when you leave the tab, hide the page or disconnect (a closed page simply
  stops renewing and the dongle stops by itself).
- **Map to edit** — the default map, the connected controller's profile (a
  controller without one starts from the default map; saving creates it), any
  stored profile, or a new profile by VID / PID / name. **Profiles** lists the
  stored ones.
- **Map editor** — per-stick radial dead zones (0–90 %) and the rules: button
  → button, axis → button (by threshold and direction), button → axis (adds a
  value while held), axis → axis (by scale, negative inverts); a rule moves its
  source unless *keep* is ticked, a target of *none* removes it. **Learn** fills
  a rule's source in from the controller (press a button or push a stick /
  trigger past 50 %). **Presets** append common rules (Nintendo A/B swap,
  West / North swap, invert left / right Y, swap sticks, left stick → d-pad,
  d-pad → left stick, triggers → L2/R2 and back). The editor checks the map the
  way the firmware does (`GamepadMap::valid()`) and lists every problem before
  **Save to dongle** is allowed. **Revert**, **Erase this profile** and **Reset
  all maps** (every profile erased, default map = identity, default rumble =
  on / 100 % / not swapped) confirm in-page.
- **Rumble row** (in the Map editor) — the map's rumble options
  (`espp::RumbleOptions`): on / off, strength 0–200 % and swap strong / weak
  motors, saved in the same `SET_MAP` as the map (a trailing `[flags][strength]`
  block). Editable only when the dongle's INFO has flags bit1 (rumble options)
  and the loaded MAP carried the block; otherwise Save sends the v1 form, which
  keeps what the dongle stores. **Test** sends `TEST_RUMBLE` (`0x08`) with the
  options as edited (not saved), only when INFO has flags bit2: the dongle plays
  its test pattern through them and the status line shows the OK or the
  dongle's reason (no controller, it cannot rumble, rumble is off in these
  settings).
- A dongle on firmware older than v4 has no button mapping: the tab says so and
  links to the Full update. A v4+ dongle that still needs the Full update
  (`needs_full_flash`) is warned that maps are not stored until then.

### System (module `7`, espp SystemService; module `8`, espp MonitorService)

- **System** — chip model / revision / cores and features, CPU clock, ESP-IDF
  and application versions, build date, ELF SHA-256, running / boot partition
  and OTA state, last reset reason, uptime, MAC, flash / PSRAM sizes, free and
  lowest-free heap, and which controls the firmware offers. Read once after
  connecting; **Refresh** re-reads it.
- **Reboot** / **Reboot into bootloader** (in-page confirmation, optional
  delay in ms) are enabled only when the firmware's capabilities allow them.
  A plain reboot behaves like the Device tab's: the console reconnects when
  the dongle re-enumerates. *Reboot into bootloader* restarts the ESP32-S3 in
  its ROM download mode: the dongle re-enumerates as the ROM's USB port, flash
  it with `idf.py flash` / `esptool`, then reset or replug it and connect
  again (the console does not reconnect on its own because the device is no
  longer the dongle).
- **Memory** — one gauge per heap region reported by the monitor service
  (used %, free, lowest free, largest block; the red mark is the peak usage
  since boot). Read when the tab is first opened and on **Refresh**. The card
  is hidden when the firmware does not advertise the monitor module.

The firmware advertises both modules with their discovery v2 protocol ids
(`espp.system`, `espp.monitor`); the console resolves them by protocol first
and falls back to the published ids `0x07` / `0x08`.

### Firmware (module `0`, espp OTA)

- **Running image** — firmware string and rollback state. A freshly flashed
  image boots *pending verify*; the console offers **Confirm this firmware**
  (cancel rollback) or **Roll back to previous** (reboots into the old image).
- **Update check** — after connecting (once the dongle's INFO is known), and
  whenever you click **Check for updates**, the console fetches
  `https://api.github.com/repos/finger563/esp-usb-ble-hid/releases/latest`
  (unauthenticated) and compares the release tag with the running firmware's
  `git describe` string (leading `v` stripped, numeric major.minor.patch). The
  result is shown on the Firmware tab and in the **Latest release** row of the
  Device status card:
  - *Up to date (v2.1.0)*;
  - *Update available: v2.2.0 (released 2026-09-10) — release notes ↗* (opens
    the GitHub release page in a new tab);
  - *Running a development build (v2.1.0-3-gabc1234) newer than the latest
    release v2.1.0* — a `-N-g<hash>` suffix counts as newer than its base tag;
  - a short note if the firmware string cannot be parsed, or if GitHub is
    unreachable / rate-limited (the check never blocks the console).

  The result is cached in `localStorage` for 10 minutes so a reload does not
  re-hit the API; **Check for updates** always re-queries.
- **Download and install vX.Y.Z** — shown when the latest release has an app
  `.bin` asset (the release workflow attaches `build/*.bin`; the console picks
  the one named after the project and ignores `bootloader` / `partition-table`
  / `ota_data` images). The asset name and size are shown next to the button.
  After an in-page confirmation the console downloads the asset from GitHub
  (progress on the OTA bar) and flashes it through exactly the same OTA path
  as a chosen file. If the browser cannot fetch the asset (network / CORS), the
  note offers a direct download link — save the file and use the picker below.
- **Update firmware** (manual) — choose the app `.bin`
  (`build/esp-usb-ble-hid.bin`), click **Flash firmware**. The console sends
  BEGIN, streams DATA chunks one at a time (waiting for each acknowledgement),
  then FINISH; the dongle validates the image and reboots. The USB link drops
  (expected), the console reconnects and prompts you to confirm the new image.
  **Abort update** stops after the in-flight chunk and discards the partial
  image.

#### Full update (Web Serial)

An OTA update only replaces the app. Moving to firmware with a new flash
layout needs one **Full update**: v3 → v4 is one (v4 adds the LittleFS
partition that stores the button maps), and so is a v4+ image that was put on a
v3 dongle over OTA (it reports `needs_full_flash` and cannot save maps). The
console prompts for it in those cases; it can also be run at any time (e.g. to
repair a dongle). It writes the bootloader, partition table, OTA data and app
at their offsets with esptool-js through the ESP32-S3 ROM's download mode. It
does **not** erase the chip: the NVS partition, which holds the paired
controllers and the settings, is kept (the console refuses images that would
overlap it), and so are the button maps of a v4 dongle.

1. **Full update** (connected dongle): the console fetches
   `firmware/latest/manifest.json` next to the page (the published release,
   shown with its version against the running one), downloads and checks the
   images, asks for confirmation, then asks the dongle (System module,
   *reboot into bootloader*) to restart into the ROM download mode. The WebUSB
   link drops; the ROM enumerates as an Espressif USB serial device
   (`303a:1001` USB-Serial-JTAG or `303a:0009` USB-OTG).
2. **Select the bootloader port and flash** (a second click: picking a serial
   port needs a user gesture): pick that port (the chooser only lists
   Espressif `303a` devices). esptool-js writes every image (compressed),
   verifies each one against the flash's MD5, then resets the chip with the
   RTC watchdog. The dongle comes back as itself and Auto-connect opens it.

A dongle that cannot reboot itself (no System module, or it does not answer):
unplug it, hold its BOOT button while plugging it in, and use step 2 directly.
If anything fails, the dongle stays in download mode until it is reset or
replugged, so step 2 can simply be retried; the log under the card shows
esptool-js's output.

Published layout (the Pages workflow puts the latest release there):
`firmware/latest/manifest.json` =
`{"version": "<release tag>", "chip": "esp32s3", "files": [{"offset": <number>, "path": "<file name>"}...]}`
plus the `.bin` files beside it. Without a manifest (a page opened from
`file://`, or nothing published yet) the card says so and offers one file
picker per offset, prefilled `0x0` / `0x8000` / `0xf000` / `0x20000`: use
`build/bootloader/bootloader.bin`, `build/partition_table/partition-table.bin`,
`build/ota_data_initial.bin` and `build/esp-usb-ble-hid.bin`. Web Serial needs
Chrome or Edge on a desktop; a page opened from `file://` cannot import the
flasher module, so flash from the hosted page or serve `web/` over
`http://localhost` (`python3 -m http.server` in `web/`).

### Crash dump (module `4`, espp CoreDumpService)

- Shows the last crash summary (or "clean boot history") and the size of the
  core dump stored in flash.
- **Download core.elf** fetches the image in chunks and saves the ELF core file
  (the flash header before the ELF magic is stripped). **Download raw image**
  saves the whole stored blob. **Erase stored dump** clears it (confirmation
  in-page).
- Decode on your PC inside an ESP-IDF shell:

  ```
  espcoredump.py info_corefile --core core.elf --core-format elf build/esp-usb-ble-hid.elf
  ```

## Log panel

The panel at the bottom shows timestamped, colour-coded messages. **Log frames
(hex)** additionally prints every request / reply frame (hex dumps are
truncated). **Clear log** empties it.

## Testing the protocol code

The first `<script id="proto">` block is DOM-free and exports its functions
when loaded under node (`module.exports`), so the frame codec, discovery (v1
and v2) / INFO / BONDS / system INFO / HEAP parsers, the settings TLV encoder
/ decoder and the firmware
version comparison (`parseVersion`, `compareFirmware`, `pickFirmwareAsset`,
`normalizeRelease`) can be unit-checked by extracting that block into a `.js`
file and `require`-ing it.

Two marked blocks inside it carry the newer pure logic: the `espp.gamepad_map`
codec (between `// --- begin gamepad map codec` and `// --- end gamepad map
codec ---`, a verbatim copy of the block in espp's
`components/gamepad_map/web/gamepad_map_console.html`; re-sync by replacing it)
and the Full update plan (between `// --- begin full update plan` and
`// --- end full update plan ---`: manifest parsing, the local-file plan,
partition table / image checks, MD5, the esptool-js read fix and the decision
to offer the Full update). `test/dongle_console_test.js` checks both against
byte vectors derived from the C++ headers (for the codec: the maps, the rumble
options and `TEST_RUMBLE`, with espp's `RumbleOptions::apply()` parity table),
plus the INFO `needs_full_flash` flag and that every inline script parses;
dependency-free:

```
node web/test/dongle_console_test.js
```

## esptool-js attribution

The Full update is powered by **esptool-js**
(<https://github.com/espressif/esptool-js>), © Espressif Systems, licensed under
the **Apache License 2.0**. To keep the console free of any runtime third-party
CDN dependency, esptool-js is **vendored same-origin**: the published bundled
build is committed here verbatim as `esptool-bundle.js` (the same file espp's
`components/usb_device/web/` vendors) and imported by `dongle_console.html` on
demand via `import('./esptool-bundle.js')`.

- Pinned version: **esptool-js 0.5.7**
- Source of the bundle:
  `https://cdn.jsdelivr.net/npm/esptool-js@0.5.7/bundle.js`

To update it, re-download that pinned URL (bump the version) and replace
`esptool-bundle.js`; the console needs the bundle to keep exporting `ESPLoader`
and `Transport`. The console patches one thing at runtime: 0.5.7's
`Transport.readLoop` drops a read that timed out while it stays queued on the
stream, losing the next chunk the chip sends (which breaks talking to a ROM
that already waits in download mode); `fixEsptoolReadTimeout()` keeps that
read for the next call. Re-check it when bumping the version.
