# Controllers

This page covers which Bluetooth controllers work with the dongle, how to check yours, and what reaches the Switch or PC. In short, the dongle is made for Bluetooth LE gamepads (so far only the Xbox Wireless Controller is tested), and Bluetooth Classic controllers (PlayStation and Nintendo controllers among them) can't connect.

## Compatibility

The dongle's ESP32-S3 chip only has a Bluetooth LE radio, so no firmware update can add Bluetooth Classic. The dongle works with controllers that act as a standard Bluetooth LE gamepad. Such a controller advertises itself as an input device and publishes a description of its buttons and sticks (its HID report map). The dongle reads that description every time the controller connects, so it isn't limited to one brand.

| Controller | Status | Notes |
|---|---|---|
| Xbox Wireless Controller, over Bluetooth LE | **Tested** | This project was built around this controller. The controllers sold with the Xbox Series X\|S use Bluetooth LE. Xbox One controllers with Bluetooth need Microsoft's Bluetooth LE firmware update first ([Xbox controllers](#xbox-controllers)). |
| Other Xbox controllers that run the Bluetooth LE firmware | Expected to work | Not tested. Back paddles are not forwarded ([What is forwarded](#what-is-forwarded)). |
| Other Bluetooth LE gamepads, for example 8BitDo or ShanWan pads in an Android mode that uses Bluetooth LE | Expected to work | Not tested. If a button lands in the wrong place, a [button map](button-mapping.md) fixes it. Please [report](#report-a-controller) what you find. |
| Sony DualShock 4 (PS4), DualSense (PS5) | Not supported | These use Bluetooth Classic. |
| Nintendo Switch Pro Controller, Joy-Con | Not supported | These use Bluetooth Classic. |
| Xbox One controllers without Bluetooth, or with Bluetooth but not yet on the Bluetooth LE firmware | Not supported | The first kind has no Bluetooth at all. The second kind uses Bluetooth Classic until it's updated. |
| Bluetooth LE keyboards, mice, remotes and media buttons | Not supported | The dongle may connect to one of these while it's pairing, but it expects a gamepad. |
| Wired USB controllers and 2.4 GHz wireless receivers | Not supported | The dongle only receives Bluetooth LE. Its USB plug is its output to the Switch or PC. |

**Tested** means the project's history shows the controller in use with this firmware. **Expected to work** means the firmware is designed to handle it, but nobody has reported on it yet. If you try one, your report moves it up the table.

## Tell Bluetooth LE from Bluetooth Classic

Many pads say "Bluetooth" and nothing more. Here are three ways to find out which kind yours uses:

- **Read the box or the manual.** Look for "Bluetooth LE", "BLE" or "Bluetooth Low Energy". A version number on its own ("Bluetooth 5.0") doesn't tell you either way.
- **Ask your phone.** A Bluetooth LE scanner app, for example nRF Connect for Mobile, lists only Bluetooth LE devices. Put the controller in pairing mode and see whether it shows up. The dongle looks for a device that advertises the Human Interface Device service (`0x1812`), or whose advertised device type (its "appearance") is Gamepad. Don't connect to the controller from the app.
- **Just try it.** Put the dongle and the controller in pairing mode together ([Pair a controller](getting-started.md#pair-a-controller)) and watch the Link block on the [web console](web-console.md)'s Device tab. If it stays on *scanning* while the controller is in pairing mode, the dongle can't see the controller. Either the controller is in the wrong mode, or it only uses Bluetooth Classic.

Pads with several modes usually pick the mode with a button held at power-on, and only their Bluetooth LE modes work here. A pad's Switch mode imitates a Switch Pro Controller, which uses Bluetooth Classic, so that mode won't connect.

## Pairing notes

[Pair a controller](getting-started.md#pair-a-controller) has the pairing steps. A few things about controllers in general:

- **The dongle connects one controller at a time.** It remembers up to five. When it isn't pairing, it connects to whichever remembered controller turns on first. Pairing a sixth controller drops the oldest pairing. For two players, use one dongle each ([Nintendo Switch](hosts.md#nintendo-switch)).
- **Pair one controller at a time,** and keep other Bluetooth keyboards, mice and remotes out of pairing mode nearby. In pairing mode the dongle takes the first input device it finds.
- **Many controllers remember only one Bluetooth host,** so pairing with your phone or PC may make a controller forget the dongle. If a controller stops reconnecting after you used it somewhere else, forget it in the web console and pair it again.

### Xbox controllers

- **Update the controller first.** Install the latest firmware with the Xbox Accessories app, on a Windows PC or an Xbox console. Xbox One controllers that have Bluetooth only get Bluetooth LE through that update.
- **Check for Bluetooth on an Xbox One controller.** On models with Bluetooth, the Xbox button is set into the front face. On the earliest models, which have no Bluetooth, the button sits in the top section with the bumpers.
- **The Share button** on newer Xbox controllers is the Capture button on the Switch.

### Other Bluetooth LE gamepads

- **Choose a Bluetooth LE mode.** The pad's manual shows how. An Android mode is the usual candidate, but check it with one of the [tests above](#tell-bluetooth-le-from-bluetooth-classic).
- **Check each control after pairing.** [Test a controller with Live input](button-mapping.md#test-a-controller-with-live-input) takes about half a minute.

## How the default layout works

The dongle reads buttons by **position**, not by the letter printed on them, because the letters differ between controller families. The four face buttons are South, East, West and North. By default each position goes to the button of the same name on the controller the dongle presents over USB. South (the bottom button) becomes A, East becomes B, West becomes X and North becomes Y.

On an Xbox controller, the labels therefore match in both modes: press A, and the Switch or PC sees A. A Pro Controller has A on the right, though. With an Xbox controller on the Switch, you confirm with the bottom button, which is where a Pro Controller has B. If you want the buttons where a Pro Controller has them, add the **Swap South / East (Nintendo A/B)** and **Swap West / North** presets ([Nintendo positions recipe](button-mapping.md#nintendo-button-positions-on-the-switch)).

[Position names](button-mapping.md#position-names) lists every position, its labels on Xbox and Nintendo controllers, and what each USB mode sends for it.

Other pads come out by position too. If yours has Nintendo-style labels, or the buttons don't land where you expect, check them with [Live input](button-mapping.md#test-a-controller-with-live-input) and fix them in a profile.

If a controller doesn't publish a readable button description, or the one it publishes doesn't look like a gamepad, the dongle reads it as an Xbox Wireless Controller.

## What is forwarded

| | Switch Pro mode | Xbox 360 mode |
|---|---|---|
| Face buttons, bumpers, stick clicks, d-pad | Yes | Yes |
| View / Menu (− / +) | Yes, as − and + | Yes, as Back and Start |
| Xbox / Home button | Yes, as Home | Yes, as Guide |
| Share / Capture | Yes, as Capture | No (an Xbox 360 controller has no such button) |
| Both sticks | Yes | Yes |
| Analog triggers | Yes, as ZL / ZR, pressed once a trigger passes half its travel (a Pro Controller's ZL / ZR are on/off buttons) | Yes, analog (LT / RT) |
| L2 / R2 reported as buttons | Yes, as ZL / ZR | Yes, as a fully pulled LT / RT |
| Back paddles | No | No |
| Battery level | Yes: the controller's battery is reported to the Switch, when the controller shares it over Bluetooth (the Xbox Wireless controller's own battery report stands in until then) | No |
| Rumble | Yes: the Switch's rumble drives the Xbox Wireless controller's motors (other controllers: not forwarded) | Yes: the PC's X-Input rumble drives them (the LED part of the report is ignored) |
| Motion controls (gyro) | No | No |
| NFC (amiibo) | No | n/a |
| Headset jack audio | No | No |

The web console's Device tab shows the controller's battery level in both modes.

The moment the link drops (the controller is switched off, its batteries die or it goes out of range), the dongle releases every button and centers both sticks, so nothing stays stuck on.

[Hosts](hosts.md) explains the two USB modes and which hosts they suit.

## Buttons in the wrong place, or missing

Pair the controller, then check every control with [Live input](button-mapping.md#test-a-controller-with-live-input):

- **A control lights the wrong position** on the Raw panel. A [profile](button-mapping.md) for that controller fixes it without any firmware change.
- **A stick works upside down.** Use the Invert presets ([Invert a stick](button-mapping.md#invert-a-stick)).
- **The triggers do nothing on the host.** If the Raw panel shows L2 / R2 lighting but the trigger bars stay at 0, your pad has on/off triggers. Current firmware sends those as ZL / ZR (or a fully pulled LT / RT); v4.0.0 did not, so [update the firmware](updating.md) ([Triggers](button-mapping.md#triggers)).
- **A control lights nothing at all** on the Raw panel. The dongle can't read that control yet, and that needs a fix in the firmware. Please [report it](#report-a-controller).

## Report a controller

If you try a controller that's not marked as tested, or it misbehaves, please [open an issue](https://github.com/finger563/esp-usb-ble-hid/issues). Include:

- **The controller.** Make, model, and the mode you started it in.
- **Its name and ID.** The Button mapping tab's *Map to edit* card shows them as *Controller: name (vid:pid)*.
- **The firmware version and USB mode.** The Device tab's Status shows both.
- **What works and what doesn't.** For example: "every button lights the right position on the Raw panel; the right stick's Y is inverted; the triggers only light L2 / R2".

For crashes and connection problems, [Reporting a bug](troubleshooting.md#reporting-a-bug) lists what else to attach.
