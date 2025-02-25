# ESP USB BLE HID

This repository contains example code for using an ESP32s3 to act as a USB-BLE
HID bridge. You would run this code for instance on a QtPy ESP32s3, connected to
a computer or other device which is a USB HID host. The QtPy / this code would
then start a BLE GATT Client to connect to a BLE HID device (this example
targets a gamepad), and will allow the wireless HID device (gamepad) to talk to
the HID Host.

## Cloning

Since this repo contains a submodule, you need to make sure you clone it
recursively, e.g. with:

``` sh
git clone --recurse-submodules https://github.com/finger563/esp-usb-ble-hid
```

Alternatively, you can always ensure the submodules are up to date after cloning
(or if you forgot to clone recursively) by running:

``` sh
git submodule update --init --recursive
```

## Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(Replace PORT with the name of the serial port to use.)

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## How To Use

The dongle can store up to 5 paired devices at a time. When it turns on / is
plugged in it will attempt to reconnect to one of those devices. If there are no
paired devices, then it will enter pairing mode.

If at any time you want to pair a new controller, simply press and hold the
button on the dongle until the LED starts pulsing blue.

### Pairing Mode

While in pairing mode, the device will scan for any BLE devices which expose a
HID service. It will connect and attempt to bond to the first device it finds.

### Reconnection Mode

When in this mode, the device will scan for the devices in its pairing list and
connect to the first one it finds.

### Connected

While connected, the device will translate xbox controller inputs received via
BLE into Nintendo Switch Pro controller inputs which will then be transmitted
over USB.

If the controller disconnects, then the dongle will re-enter reconnection mode.

## Output

![Image](https://github.com/user-attachments/assets/c6e0bed9-60e5-4ed4-9a31-0082b0b804c6)

## Helpful Links

The links below were invaluable in developing the switch pro implemenation
within this repo such that it would work on MacOS, Android, iOS, and (most
importantly) the Nintendo Switch.

* https://github.com/Brikwerk/nxbt/blob/master/nxbt/controller/protocol.py
* https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/bluetooth_hid_subcommands_notes.md
* https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/USB-HID-Notes.md
* https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/spi_flash_notes.md
* https://github.com/EasyConNS/BlueCon-esp32/tree/master/components/joycon
* https://github.com/mzyy94/nscon/blob/master/nscon.go
* https://www.mzyy94.com/blog/2020/03/20/nintendo-switch-pro-controller-usb-gadget/

