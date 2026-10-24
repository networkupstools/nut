/* CyberPower HID text transport. GNU GPL version 2 or later. */
#ifndef POWERP_USB_H
#define POWERP_USB_H

int powerpanel_usb_init(void);
void powerpanel_usb_cleanup(void);
int powerpanel_usb_connected(void);
ssize_t powerpanel_usb_command(const char *command, char *reply, size_t size);

#endif
