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
- No build step, no CDN: the page is self-contained (inline CSS + JS). Copy it
  anywhere. Its only network access is the optional firmware-update check
  against GitHub Releases (see the Firmware tab below); everything else works
  offline.
- Click **Connect dongle** and pick the device. The dongle enumerates as a
  *Nintendo Switch Pro Controller* (VID `057E`, PID `2009`); **Pick any USB
  device…** lists every device if you are on a modified build.
- On Linux you may need a udev rule granting your user access to `057e:2009`.

On connect the console sends a dispatcher discovery request and shows the
device name / firmware in the header. A tab whose module the firmware does not
advertise is disabled.

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
- **Settings** — stick Y-axis inversion, A/B and X/Y swaps, radial stick
  deadzone (0–50 %), status-LED brightness, BLE device name (applies after a
  reboot). **Apply changes** sends only the keys you changed; the form always
  re-renders from the dongle's reply. **Reset to defaults** restores the
  firmware defaults.
- **Actions** — *Start pairing* (same as holding the dongle's button), *Forget
  all controllers* (clears every BLE bond, with an in-page confirmation),
  *Reboot dongle* (the console reconnects automatically when the dongle
  re-enumerates).

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
when loaded under node (`module.exports`), so the frame codec, discovery /
INFO / BONDS parsers, the settings TLV encoder / decoder and the firmware
version comparison (`parseVersion`, `compareFirmware`, `pickFirmwareAsset`,
`normalizeRelease`) can be unit-checked by extracting that block into a `.js`
file and `require`-ing it.
