
#include <usb.h> /* libusb */

extern int libusb_open(HIDDevice *curDevice, MatchFlags *flg, unsigned char *ReportDesc, int mode);
void libusb_close(HIDDevice *curDevice);

extern usb_dev_handle *udev;

//extern int usb_get_descriptor(int type, int len, char *report);
extern int libusb_get_report(int ReportId, unsigned char *raw_buf, int ReportSize );

extern int libusb_set_report(int ReportId, unsigned char *raw_buf, int ReportSize );
extern int libusb_get_string(int StringIdx, char *string);
extern int libusb_get_interrupt(unsigned char *buf, int bufsize, int timeout);
