# ESP USB BLE HID

Example code for using BLE gamepad (such as Xbox wireless controller) with the
Nintendo Switch via a USB dongle.

This repository contains example code for using an ESP32s3 to act as a USB-BLE
HID bridge. You would run this code for instance on a QtPy ESP32s3 or a LilyGo
T-Dongle S3, connected to a computer or other device which is a USB HID host.
The main HID host that this targets is the Nintendo Switch. The QtPy / this code
would then start a BLE GATT Client to connect to a BLE HID device (this example
targets a gamepad), and will allow the wireless HID device (gamepad) to talk to
the HID Host.

![image](https://github.com/user-attachments/assets/d76558db-c34e-48d4-9771-06fa4ebdb05a)

https://github.com/user-attachments/assets/a0789d38-bd0e-4215-bf2c-ebedd9958495

![image](https://github.com/user-attachments/assets/9dead4bb-c41c-43a0-b833-d14e99000340)

https://github.com/user-attachments/assets/c81b947a-24a1-4a44-b5d0-5d4c274beb93


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

### Note about system power

The switch turns off its USB-C port when it enters sleep mode. This means that
while the Switch Dock's USB-A port still has power, the dongle will not properly
mount as a usb device until the Switch comes out of sleep. 

For this reason, you cannot use this dongle or the associated BLE controller to
power on your switch unfortunately. The only way (currently) to remotely wake
your switch is via Bluetooth Classic.

That being said, I have read online that if you plug a usb-to-ethernet adapter
into your Switch Dock, then the Switch may keep its USB-C port awake during
sleep.

## Output

https://github.com/user-attachments/assets/a0789d38-bd0e-4215-bf2c-ebedd9958495

![CleanShot 2025-02-25 at 08 54 49](https://github.com/user-attachments/assets/d06a53cb-c20c-4de8-9987-38a7bc05b60a)

![CleanShot 2025-02-25 at 09 02 40](https://github.com/user-attachments/assets/6c3820d1-b9f0-4188-96a6-0d1d8b44e1fb)

![CleanShot 2025-02-25 at 09 03 03](https://github.com/user-attachments/assets/89f524e4-1737-4aec-92ac-e3a64f69c6fe)

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

