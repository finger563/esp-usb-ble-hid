# Troubleshooting

Look up your symptom below. Many answers start in the [web console](web-console.md)'s **Device** tab, where the **Link** block shows where the connection is stuck.

The Link block has three lines:

- **Controller link** shows how far the Bluetooth connection has got (scanning, connecting, encrypting, subscribing, subscribed) and the dongle's own note on the last thing that happened.
- **BLE input** shows whether input is arriving from the controller.
- **USB output** shows whether the Switch or PC is taking the dongle's reports.

<!-- screenshot: the Device tab's Link block for a working link (subscribed, notifications counting up, handshake done) -->

## Pairing and connection

### The dongle won't pair with my controller

- **Check that the dongle is in pairing mode.** Its LED pulses blue quickly, about once a second, and the Device tab shows a **Pairing mode** badge. After you hold the button for 3 seconds, pairing mode lasts about 30 seconds. If you missed it, hold the button again.
- **Check that the controller is in pairing mode,** not just switched on. On an Xbox Wireless Controller, hold the pair button on the top edge until the Xbox button flashes quickly. Steps for other controllers are in [Controllers](controllers.md).
- **Check that it is a Bluetooth LE controller.** PlayStation 4 and 5 controllers, the Switch Pro Controller and Joy-Cons use Bluetooth Classic, and the dongle can't pair with them. Some controllers have several modes, and only some of them use Bluetooth LE. See the [compatibility table](controllers.md#compatibility).
- **Check what else is nearby.** The dongle pairs with the first Bluetooth LE controller or input device it finds. Turn off your other controllers, and keep Bluetooth keyboards, mice and remotes out of pairing mode. Also turn off a phone, PC or Xbox console the controller was used with, so it doesn't grab the controller first.
- **Turn off the controller that is connected now.** While a controller is connected, the LED stays steady, so you can't tell whether pairing has started, and the dongle looks for a new controller for only a few seconds instead of the full 30.
- **Look at the Link block.** The Controller link line says what the dongle is doing, for example "scanning" or "connecting".

### It paired with the wrong device

On the Device tab, find the device under **Paired controllers** and click **Forget** on its row. Switch that device off, then [pair your controller](getting-started.md#pair-a-controller) again.

### The controller connects, then drops again

The **Controller link** line shows which step fails and why. For example: "controller did not encrypt the link within 15 s; disconnected". The usual cause is an old pairing that one side has forgotten. To start fresh on both sides:

1. Under **Paired controllers**, click **Forget** next to the controller.
2. Put the controller in pairing mode and [pair it again](getting-started.md#pair-a-controller).

A low battery or a long distance can cause drops too. Try again with the controller charged and close to the dongle.

### The controller doesn't reconnect by itself

- **Turn the controller on.** On an Xbox controller, press the Xbox button. The dongle keeps looking for its paired controllers, but most controllers look for their host only for a while after you turn them on.
- **Pair it again if you used it somewhere else.** Many controllers remember only one Bluetooth host, or only a few. Pairing the controller with a phone or PC can make it forget the dongle.
- **Check that the dongle still has it.** The dongle remembers five controllers. Pairing a sixth replaces the one paired longest ago. Look under **Paired controllers** on the Device tab.
- **Look at the Link block.** On the Device tab, while the dongle is looking for its controllers, the **Controller link** line names any gamepad it hears that is *not* paired, with its kind of address: *scanning; a gamepad that is not paired is advertising: AA:BB:CC:DD:EE:FF (random, resolvable private)*. If that is your controller, the dongle doesn't recognise the address it is using now. Please [report it](#reporting-a-bug) with that line and the address listed under **Paired controllers**. Forgetting the controller and pairing it again gets it working in the meantime.

### The wrong controller connects

The dongle uses one controller at a time and connects to whichever paired controller it hears first. Turn off the controllers you aren't using.

### The LED seems to be off

That is normal while the dongle is waiting for a paired controller. The slow pulse (once every 3 seconds) is very faint by default, 1 % of the full brightness, so it doesn't light up a room. While a controller is connected, the LED glows steadily at 25 % unless that level is set to 0. You can raise both levels in [Settings](web-console.md#settings). All the LED patterns are listed in [LED and button](hardware.md#led-and-button).

## Input

### The controller is connected, but nothing happens

Check the Link block on the Device tab:

- **BLE input says "none since connect", or the last input was a long time ago.** The controller isn't sending anything. Press a button or switch the controller off and on. If it never sends, forget the controller and pair it again.
- **Controller link stays at "encrypting" or "subscribing".** The connection is still being set up. If that doesn't finish within about 15 seconds, the dongle drops the link and tries again. If this keeps happening, forget the controller and pair it again.
- **USB output says "waiting for host handshake"** (Switch Pro mode). The Switch hasn't accepted the dongle yet. See [The Switch doesn't see the controller](#the-switch-doesnt-see-the-controller).
- **USB output says "host is not reading X-Input reports"** (Xbox 360 mode). Nothing on the host is reading the controller. Either the host has no Xbox 360 driver, as on a Mac, or it needs the other mode. See [Choose a mode](hosts.md#choose-a-mode).

Then open the **Button mapping** tab and watch **Live input**. The **Raw** panel shows what the controller sends, and the **Mapped** panel shows what goes to the Switch or PC. If Raw moves but Mapped doesn't, a mapping rule is moving or removing that control. See [Button mapping](button-mapping.md).

### A and B (or X and Y) are in the wrong place

By default the dongle places buttons by position the way an Xbox controller labels them: bottom is A, right is B, left is X and top is Y. With an Xbox controller the labels match. Switch games expect Nintendo's layout, though, where A is on the right, and on a controller with Nintendo-style labels the letters don't match at all. Either way, the **Swap South / East (Nintendo A/B)** and **Swap West / North** presets give you Nintendo's positions. See [button mapping recipes](button-mapping.md#recipes).

### A stick drifts or moves the wrong way

- **Drift** (the stick moves on its own): set a dead zone for that stick on the Button mapping tab. You can set each stick from 0 to 90 %.
- **Up and down are reversed:** use the **Invert left Y** or **Invert right Y** preset.

Both are covered in [Button mapping](button-mapping.md#recipes).

### The triggers do nothing, or a rule for L2 / R2 does nothing

Firmware v4.0.0 sent only the controller's **trigger axes** to the host. Controllers that report their triggers as plain L2 / R2 buttons, and rules that target L2 / R2, did nothing on the Switch or PC. Later firmware sends both. [Update the firmware](updating.md), then check again.

To see which kind your controller has, watch **Live input** on the Button mapping tab while you press a trigger. If a trigger bar moves, the trigger is an axis. If L2 or R2 lights up instead, it is a button. Both work on current firmware ([Triggers](button-mapping.md#triggers)).

### Some buttons do nothing

Press the button while you watch **Live input** on the Button mapping tab:

- **Nothing lights up on the Raw panel.** The dongle can't read that button from your controller. Back paddles, for example, aren't read. See [What is forwarded](controllers.md#what-is-forwarded). If it is a normal button, please [report it](#reporting-a-bug) and name your controller.
- **It lights up on Raw but not on Mapped.** A mapping rule moves it elsewhere, or a rule with the target "none" removes it. Edit or remove that rule.
- **In Xbox 360 mode, Share / Capture does nothing.** X-Input has no such button. Map it to another button if you need it.

### Rumble is missing, too strong, too weak or on the wrong side

- **Check that the controller can rumble.** The Device tab's **Rumble** row must say *supported*. The dongle drives only controllers that describe a rumble report ([What is forwarded](controllers.md#what-is-forwarded)).
- **Check the controller's Rumble row** on the Button mapping tab: load the map the controller uses (its profile, or the default map) and look at **on** and **Strength**. If the Device tab's **Test rumble** says *rumble is off for this controller (Button mapping tab)*, rumble is switched off or set to 0 % there.
- **Too strong or too weak:** move **Strength** and click **Test** to feel it, then **Save to dongle**. 100 % is what the host asks for.
- **The heavy rumble comes from the wrong grip:** with swap unticked, click **Test**. The first pulse is meant for the strong motor in the left grip. If you feel it on the right, tick **swap strong / weak motors**, test again and save.

See [Rumble](button-mapping.md#rumble) for what each control does.

## Nintendo Switch

### The Switch doesn't see the controller

- **Turn on Pro Controller Wired Communication** on the Switch. See [Nintendo Switch](hosts.md#nintendo-switch).
- **Check the mode.** The Device tab's **USB controller** row must say **Switch Pro**, because the Switch doesn't recognize the Xbox 360 mode. To change it, see [Switch to Xbox 360 mode](hosts.md#switch-to-xbox-360-mode), which also covers going back. On the release firmware, **Reset to defaults** in Settings also brings back Switch Pro mode, once the dongle restarts.
- **Wake the Switch first.** The dongle only has power while the Switch is awake.
- **Unplug the dongle and plug it back in** if the Link block's USB output line stays at "waiting for host handshake".

On the T-Dongle-S3's screen, the host icon shows the USB side. It is in color with a USB badge once the Switch has set the dongle up over USB, and a white silhouette until then.

### The dongle disappeared on the Change Grip/Order screen

The Switch sometimes drops the dongle on that screen. The dongle reconnects itself after about 2.5 seconds, so wait a few seconds. If it doesn't come back, unplug it and plug it back in. More about that screen: [Change Grip/Order](hosts.md#change-griporder).

### Nothing works after the Switch wakes up

The dongle turns off while the Switch sleeps and starts again when the Switch wakes. Turn your controller on (on an Xbox controller, press the Xbox button) and it reconnects within a few seconds.

If you had installed a firmware update and hadn't confirmed it yet, the dongle went back to the previous version when it lost power. See [After an update, the old firmware came back](#after-an-update-the-old-firmware-came-back).

## PC, Steam Deck and Android

### Windows or Steam doesn't see an Xbox controller

- **Switch the dongle to Xbox 360 mode.** It starts out as a Switch Pro Controller. See [Switch to Xbox 360 mode](hosts.md#switch-to-xbox-360-mode).
- **Restart the dongle after changing the mode.** The change takes effect only after a restart. Until then, the Device tab's **USB controller** row says "reboot to apply".
- **Check the Link block.** If USB output says "X-Input reports taken by the host", the PC is reading the controller. On Windows, you can also check that the controller appears in **Game Controllers** (run `joy.cpl`).

### Steam shows a Switch Pro Controller

The dongle is still in Switch Pro mode. [Switch it to Xbox 360 mode](hosts.md#switch-to-xbox-360-mode).

### The Share or Capture button does nothing

In Xbox 360 mode that is expected, because X-Input has no Capture button. Map Share / Capture to another button on the [Button mapping](button-mapping.md) tab if you need it.

### On a Mac, the Device tab says "host is not reading X-Input reports"

That is expected. macOS has no Xbox 360 driver, so nothing reads the controller in Xbox 360 mode. The web console still works. Plug the dongle into your PC to play, or use Switch Pro mode on the Mac. See [macOS and iOS](hosts.md#macos-and-ios).

## Updates

### The programmer can't find the dongle

- **Put the dongle in download mode.** Unplug it, hold its button while you plug it back in, then let go and run the programmer again. Once this firmware is running, the programmer can only reach the dongle in download mode. See [Download mode](hardware.md#download-mode).
- **Close other programs that might hold the port,** such as a second programmer window, a browser tab running the console's Full update, or a serial monitor.
- **Try another USB port,** directly on the computer rather than through a hub.
- **On Linux, check permissions.** If you see "permission denied", add the rules in [Linux permissions](hosts.md#linux-permissions).
- **Name the port yourself** with `--port`, as shown in [Install the firmware](getting-started.md#install-the-firmware). To find it, look in Device Manager > Ports on Windows, run `ls /dev/cu.usbmodem*` on macOS, or run `ls /dev/ttyACM*` on Linux.

A failed or interrupted install doesn't harm the dongle. Download mode is built into the chip, so you can always start again.

### macOS won't open the programmer

- **"cannot be opened because the developer cannot be verified"** or **"Apple could not verify … is free of malware":** open **System Settings > Privacy & Security**, click **Open Anyway**, and run the programmer again. Or, in Terminal in your Downloads folder, remove the downloaded-file flag with `xattr -d com.apple.quarantine esp-usb-ble-hid_programmer_*_macos.bin` and run it again.
- **"permission denied":** run `chmod +x` on the file first, as shown in [Install the firmware](getting-started.md#install-the-firmware).
- **"bad CPU type in executable":** this is an Intel Mac. The macOS programmer runs only on Apple-silicon Macs, so use a Windows or Linux computer.

### The console says a Full update is needed

When you see **One-time Full update needed**, the dongle runs firmware older than v4. Or it got v4 over USB on top of the old flash layout, which can't store button maps. A normal update can't change the flash layout, so this one-time update rewrites it from the browser. Your paired controllers and settings are kept. Follow [Full update](updating.md#full-update).

### After an update, the old firmware came back

A new firmware stays on trial until you confirm it. If the dongle restarts before you do, it goes back to the previous version. Unplugging it, rebooting it and the Switch going to sleep all count as a restart. Install the update again, and when the console asks, click **Confirm this firmware** on the Firmware tab before you unplug the dongle. See [Confirm or roll back](updating.md#confirm-or-roll-back).

### My button maps are gone after a reboot

- **Check that you saved them.** Changes in the map editor reach the dongle only when you click **Save to dongle**.
- **If the Button mapping tab says "Maps are not saved yet",** the dongle runs v4 on the old flash layout and has nowhere to store maps. One [Full update](updating.md#full-update) fixes that.
- **If you went back to older firmware after editing maps** (a rollback, an update you didn't confirm, or an older release), the older firmware couldn't read them. It set them aside as `gamepad_profiles.bin.unreadable` and started with no profiles. See [Roll back after editing maps](updating.md#roll-back-after-editing-maps).

### I installed a release on a board that isn't a T-Dongle-S3

The release firmware is built for the T-Dongle-S3 only.

- **If you installed it with Download and install and haven't confirmed it yet,** unplug the dongle and plug it back in. It goes back to the firmware it had before.
- **Otherwise** (you confirmed it, or you installed it with the Full update or the programmer, which don't roll back on a restart), put the board in [download mode](hardware.md#download-mode) and flash a build for your board. See [Build for your board](building.md#build-for-your-board).

## Web console

### Connect dongle lists nothing

- **Use desktop Chrome, Edge or Brave.** The console needs WebUSB, which Firefox, Safari and iPhone or iPad browsers don't have. Opening the page is covered in [Web console](web-console.md).
- **Check that the dongle is running its firmware,** not sitting in download mode. In download mode it is a serial port, not the dongle. Unplug it and plug it back in without holding the button.
- **On Linux, check permissions.** Add the rules in [Linux permissions](hosts.md#linux-permissions).
- **Using a build with a different USB ID?** Click **Pick any USB device…** to see every device.

### The dongle is listed but won't open

- **Close other tabs or programs using it,** such as a second console tab. Only one page can use the dongle at a time.
- **On Linux,** check [Linux permissions](hosts.md#linux-permissions). A Snap or Flatpak browser can also block USB access.
- **Check the firmware.** If the console's log says the device has no vendor-specific interface, the dongle runs v2.1.0 or older, which has no console. Move it to the current firmware with a [Full update](updating.md#full-update).

### The console stopped reconnecting after I changed the USB controller type

In the other mode the dongle has a different USB identity, and the browser asks for permission once per identity. Click **Connect dongle** and pick the new entry: **Xbox 360 Controller** or **Pro Controller**. After that, the console reconnects to either one by itself. See [Auto-connect](web-console.md#auto-connect).

### The console doesn't come back after Reboot into bootloader

That is expected. **Reboot into bootloader** puts the dongle in download mode, where it is a different USB device, and it stays there until it is flashed or unplugged. Unplug it and plug it back in, and the console finds it again.

### A setting didn't take effect

The **USB controller** type and the **BLE device name** apply only after the dongle restarts. Click **Reboot dongle** on the Device tab, or unplug the dongle and plug it back in. For every setting and when it applies, see [Settings](web-console.md#settings).

## FAQ

### Does rumble work?

Yes, on controllers that describe a rumble report in their report map, as the Xbox Wireless Controller does: the host's rumble (the Switch's, or a PC's in Xbox 360 mode) plays on the controller's motors. The [web console](web-console.md#status)'s **Rumble** row says whether the connected controller does, and **Test rumble** plays a short pattern to check it. Nothing is written to a controller that doesn't describe one.

Rumble stops when the host sends a stop, goes to sleep or is unplugged, and when the controller disconnects. See [What is forwarded](controllers.md#what-is-forwarded).

You can also turn rumble off, make it weaker or stronger, or swap the motors, for each controller model, on the Button mapping tab ([Rumble](button-mapping.md#rumble)).

### Do motion controls work?

No. The dongle passes on buttons, sticks and triggers, plus the battery level on the Switch. It doesn't pass on motion, so games that need gyro aiming won't get it.

### Can the dongle wake my Switch?

No. The Switch stops powering the dongle while it sleeps. Wake it with its power button or a Joy-Con, then turn your controller on. See [Sleep and wake](hosts.md#sleep-and-wake).

### How many controllers can it remember?

Five. It uses one at a time: whichever paired controller turns on first. Pairing a sixth replaces the one paired longest ago.

### Can two people play?

Yes, with one dongle per player. Each dongle carries one controller and has its own identity. See [Play with more than one player](hosts.md#play-with-more-than-one-player).

### Do I need a computer every time?

No. You need one for the first install, and later only if you want to change settings, remap buttons or update the firmware. Day to day, you just plug the dongle in and turn your controller on.

### Will an update erase my pairings, settings or button maps?

No. Normal updates from the console replace only the firmware. The one-time Full update keeps your paired controllers, settings and button maps too. See [Updating](updating.md). Going back to older firmware after you've edited maps is the one case that loses them: see [Roll back after editing maps](updating.md#roll-back-after-editing-maps).

### Does it add input lag?

The dongle asks the controller for a 15 ms Bluetooth connection interval. In Switch Pro mode it sends a report every 8 ms, as a real Pro Controller does. In Xbox 360 mode it sends each change as it arrives. The total delay from button to screen hasn't been measured.

### Does the web console send my data anywhere?

No. The console talks to the dongle directly over USB, and nothing about your dongle leaves your computer. Besides loading the page itself, it goes online only to check GitHub for the newest release and to download firmware when you update. See [What the page sends over the network](web-console.md#what-the-page-sends-over-the-network).

### Does it work on other consoles?

Xbox and PlayStation consoles aren't supported. The project hasn't been tested on a Nintendo Switch 2. If you try it there, please [report what happens](#reporting-a-bug).

### Can I use a PlayStation controller, a Switch Pro Controller or Joy-Cons?

No. They use Bluetooth Classic, and the dongle supports only Bluetooth LE. See the [compatibility table](controllers.md#compatibility).

## Reporting a bug

Open an issue at [github.com/finger563/esp-usb-ble-hid/issues](https://github.com/finger563/esp-usb-ble-hid/issues) and include:

- **The firmware version and board**, from the Device tab's **Firmware** and **Hardware** rows.
- **Your controller**: its make and model, and the name the dongle shows under **Paired controllers**.
- **What you plugged it into**, and the mode from the Device tab's **USB controller** row (Switch Pro or Xbox 360).
- **The Link block.** Copy its three lines (Controller link, BLE input, USB output), or take a screenshot, while the problem is happening.
- **What you did, what you expected, and what happened instead.**
- **A crash dump, if there is one.** If the Crash dump tab shows a crash, click **Download core.elf** and attach the file.

If you can't connect to the console at all, say so, and give the version you installed if you know it.

For developers: to decode a crash dump from a release build, use the `esp-usb-ble-hid.elf` attached to the same GitHub release. Run this in an ESP-IDF shell:

```sh
espcoredump.py info_corefile --core core.elf --core-format elf esp-usb-ble-hid.elf
```

For your own build, use `build/esp-usb-ble-hid.elf` from that build instead.
