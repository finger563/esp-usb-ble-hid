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
- No build step, no network access: the page is self-contained (inline CSS + JS,
  no CDN). Copy it anywhere.
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
- **Update firmware** — choose the app `.bin` (`build/esp-usb-ble-hid.bin`),
  click **Flash firmware**. The console sends BEGIN, streams DATA chunks one at
  a time (waiting for each acknowledgement), then FINISH; the dongle validates
  the image and reboots. The USB link drops (expected), the console reconnects
  and prompts you to confirm the new image. **Abort update** stops after the
  in-flight chunk and discards the partial image.

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
INFO parsers and the settings TLV encoder / decoder can be unit-checked by
extracting that block into a `.js` file and `require`-ing it.
