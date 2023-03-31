Desc: Method for resetting unreliable USB UPS interfaces  
File: scripts/usb_resetter/README  
Date: 30 Mar 2023  
Auth: Orsiris de Jong <ozy@netpower.fr> - NetInvent SASU  

Some cheaper USB UPS have the same kind of unreliable USB to serial interface, being a Cypress Semiconductor USB to Serial / UNNO TECH USB to Serial.  
Most of them use `blazer_usb` driver, and sometimes the driver can't start because it can't communicate with the UPS.

Unplugging and plugging the USB port usually fixes this, but that's not convenient.

That's where usb_resetter comes in handy. (see https://github.com/netinvent/usb_resetter for more info)  
Grab a copy via pip with `pip install usb_resetter` or make a plain install with
```
curl -o /usr/local/bin/usb_resetter -L https://raw.githubusercontent.com/netinvent/usb_resetter/main/usb_resetter/usb_resetter.py && chmod +x /usr/local/bin/usb_resetter
```

Once you got the script, identify the USB UPS with

```
usb_resetter --list
```

In our case, we could find something like `Found device 0665:5161 at /dev/bus/usb/001/002 Manufacturer=INNO TECH, Product=USB to Serial`

usb_restter can work in three different ways:
- Reset device itself
- Reset the hub the device is attached to
- Reset all USB controllers

A simple USB device reset isn't sufficient for those UPS devices.

We'll need to reset the hub it's attached to.

The command for doing so is:
```
usb_resetter --reset-hub --device 0665:5161
```

Bear in mind that this will reset other devices connected to the same hub. While this isn't a problem for a keyboard / mouse, it might be for a USB storage device.  
On some hardware, each USB plug gets it's own hub. On others, two or more USB plus share one hub.
A good practice would be to isolate the USB UPS on a hub without any other device in order to not interfere with other hardware, or associate it on a hub where a non critical device is already plugged.

Getting the hub your device is attached to can be done with:
```
usb_resetter --list-hubs --device 0665:5161
```


The easiest way to integrate with nut-driver is to modify the systemd service file with the following line:
```
ExecStartPre=/usr/local/bin/usb_reset.py --reset-hub --device 0665:5161
```

This way, every time the nut-driver service is reloaded, the USB UPS is reset.
