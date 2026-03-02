/*! 
 * @file libwinhid.h
 * @brief Generic USB communication backend (Windows HID API, phase 1)
 */

#ifndef NUT_LIBWINHID_H_SEEN
#define NUT_LIBWINHID_H_SEEN 1

#include "nut_libusb.h"

#ifdef WIN32
extern usb_communication_subdriver_t winhid_subdriver;
#endif

#endif /* NUT_LIBWINHID_H_SEEN */
