/*
 * Copyright © 2001-2011 Stéphane Raimbault <stephane.raimbault@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */




#ifndef MODBUS_VERSION_H
#define MODBUS_VERSION_H

/* The major version, (1, if %LIBMODBUS_VERSION is 1.2.3) */
#define LIBMODBUS_VERSION_MAJOR (3)

/* The minor version (2, if %LIBMODBUS_VERSION is 1.2.3) */
#define LIBMODBUS_VERSION_MINOR (1)

/* The micro version (3, if %LIBMODBUS_VERSION is 1.2.3) */
#define LIBMODBUS_VERSION_MICRO (1)

/* The full version, like 1.2.3 */
#define LIBMODBUS_VERSION        3.1.1

/* The full version, in string form (suited for string concatenation)
 */
#define LIBMODBUS_VERSION_STRING "3.1.1"

/* Numerically encoded version, like 0x010203 */
#define LIBMODBUS_VERSION_HEX ((LIBMODBUS_VERSION_MAJOR << 24) |  \
                               (LIBMODBUS_VERSION_MINOR << 16) |  \
                               (LIBMODBUS_VERSION_MICRO << 8))

/* Evaluates to True if the version is greater than @major, @minor and @micro
 */
#define LIBMODBUS_VERSION_CHECK(major,minor,micro)      \
    (LIBMODBUS_VERSION_MAJOR > (major) ||               \
     (LIBMODBUS_VERSION_MAJOR == (major) &&             \
      LIBMODBUS_VERSION_MINOR > (minor)) ||             \
     (LIBMODBUS_VERSION_MAJOR == (major) &&             \
      LIBMODBUS_VERSION_MINOR == (minor) &&             \
      LIBMODBUS_VERSION_MICRO >= (micro)))

#endif /* MODBUS_VERSION_H */

#ifndef MODBUS_H
#define MODBUS_H

/* Add this for macros that defined unix flavor */
#if (defined(__unix__) || defined(unix)) && !defined(USG)
#include <sys/param.h>
#endif

#ifndef _MSC_VER
#include <stdint.h>
#else
#include "stdint.h"
#endif

#if defined(_MSC_VER)
# if defined(DLLBUILD)
/* define DLLBUILD when building the DLL */
#  define MODBUS_API __declspec(dllexport)
# else
#  define MODBUS_API __declspec(dllimport)
# endif
#else
# define MODBUS_API
#endif

#ifdef  __cplusplus
# define MODBUS_BEGIN_DECLS  extern "C" {
# define MODBUS_END_DECLS    }
#else
# define MODBUS_BEGIN_DECLS
# define MODBUS_END_DECLS
#endif

MODBUS_BEGIN_DECLS

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef OFF
#define OFF 0
#endif

#ifndef ON
#define ON 1
#endif

/* Modbus function codes */
#define MODBUS_FC_READ_COILS                0x01
#define MODBUS_FC_READ_DISCRETE_INPUTS      0x02
#define MODBUS_FC_READ_HOLDING_REGISTERS    0x03
#define MODBUS_FC_READ_INPUT_REGISTERS      0x04
#define MODBUS_FC_WRITE_SINGLE_COIL         0x05
#define MODBUS_FC_WRITE_SINGLE_REGISTER     0x06
#define MODBUS_FC_READ_EXCEPTION_STATUS     0x07
#define MODBUS_FC_WRITE_MULTIPLE_COILS      0x0F
#define MODBUS_FC_WRITE_MULTIPLE_REGISTERS  0x10
#define MODBUS_FC_REPORT_SLAVE_ID           0x11
#define MODBUS_FC_MASK_WRITE_REGISTER       0x16
#define MODBUS_FC_WRITE_AND_READ_REGISTERS  0x17

#define MODBUS_BROADCAST_ADDRESS    0

/* Modbus_Application_Protocol_V1_1b.pdf (chapter 6 section 1 page 12)
 * Quantity of Coils to read (2 bytes): 1 to 2000 (0x7D0)
 * (chapter 6 section 11 page 29)
 * Quantity of Coils to write (2 bytes): 1 to 1968 (0x7B0)
 */
#define MODBUS_MAX_READ_BITS              2000
#define MODBUS_MAX_WRITE_BITS             1968

/* Modbus_Application_Protocol_V1_1b.pdf (chapter 6 section 3 page 15)
 * Quantity of Registers to read (2 bytes): 1 to 125 (0x7D)
 * (chapter 6 section 12 page 31)
 * Quantity of Registers to write (2 bytes) 1 to 123 (0x7B)
 * (chapter 6 section 17 page 38)
 * Quantity of Registers to write in R/W registers (2 bytes) 1 to 121 (0x79)
 */
#define MODBUS_MAX_READ_REGISTERS          125
#define MODBUS_MAX_WRITE_REGISTERS         123
#define MODBUS_MAX_WR_WRITE_REGISTERS      121
#define MODBUS_MAX_WR_READ_REGISTERS       125

/* The size of the MODBUS PDU is limited by the size constraint inherited from
 * the first MODBUS implementation on Serial Line network (max. RS485 ADU = 256
 * bytes). Therefore, MODBUS PDU for serial line communication = 256 - Server
 * address (1 byte) - CRC (2 bytes) = 253 bytes.
 *
 * Consequently:
 * - RS232 / RS485 ADU = 253 bytes + Server address (1 byte) + CRC (2 bytes) =
 *   256 bytes.
 * - TCP MODBUS ADU = 253 bytes + MBAP (7 bytes) = 260 bytes.
 */
#define MODBUS_MAX_PDU_LENGTH              253

/* Random number to avoid errno conflicts */
#define MODBUS_ENOBASE 112345678

/* Protocol exceptions */
enum {
    MODBUS_EXCEPTION_ILLEGAL_FUNCTION = 0x01,
    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
    MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE,
    MODBUS_EXCEPTION_ACKNOWLEDGE,
    MODBUS_EXCEPTION_SLAVE_OR_SERVER_BUSY,
    MODBUS_EXCEPTION_NEGATIVE_ACKNOWLEDGE,
    MODBUS_EXCEPTION_MEMORY_PARITY,
    MODBUS_EXCEPTION_NOT_DEFINED,
    MODBUS_EXCEPTION_GATEWAY_PATH,
    MODBUS_EXCEPTION_GATEWAY_TARGET,
    MODBUS_EXCEPTION_MAX
};

#define EMBXILFUN  (MODBUS_ENOBASE + MODBUS_EXCEPTION_ILLEGAL_FUNCTION)
#define EMBXILADD  (MODBUS_ENOBASE + MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS)
#define EMBXILVAL  (MODBUS_ENOBASE + MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE)
#define EMBXSFAIL  (MODBUS_ENOBASE + MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE)
#define EMBXACK    (MODBUS_ENOBASE + MODBUS_EXCEPTION_ACKNOWLEDGE)
#define EMBXSBUSY  (MODBUS_ENOBASE + MODBUS_EXCEPTION_SLAVE_OR_SERVER_BUSY)
#define EMBXNACK   (MODBUS_ENOBASE + MODBUS_EXCEPTION_NEGATIVE_ACKNOWLEDGE)
#define EMBXMEMPAR (MODBUS_ENOBASE + MODBUS_EXCEPTION_MEMORY_PARITY)
#define EMBXGPATH  (MODBUS_ENOBASE + MODBUS_EXCEPTION_GATEWAY_PATH)
#define EMBXGTAR   (MODBUS_ENOBASE + MODBUS_EXCEPTION_GATEWAY_TARGET)

/* Native libmodbus error codes */
#define EMBBADCRC  (EMBXGTAR + 1)
#define EMBBADDATA (EMBXGTAR + 2)
#define EMBBADEXC  (EMBXGTAR + 3)
#define EMBUNKEXC  (EMBXGTAR + 4)
#define EMBMDATA   (EMBXGTAR + 5)
#define EMBBADSLAVE (EMBXGTAR + 6)

extern const unsigned int libmodbus_version_major;
extern const unsigned int libmodbus_version_minor;
extern const unsigned int libmodbus_version_micro;

typedef struct _modbus modbus_t;

typedef struct {
    int nb_bits;
    int nb_input_bits;
    int nb_input_registers;
    int nb_registers;
    uint8_t *tab_bits;
    uint8_t *tab_input_bits;
    uint16_t *tab_input_registers;
    uint16_t *tab_registers;
} modbus_mapping_t;

typedef enum
{
    MODBUS_ERROR_RECOVERY_NONE          = 0,
    MODBUS_ERROR_RECOVERY_LINK          = (1<<1),
    MODBUS_ERROR_RECOVERY_PROTOCOL      = (1<<2)
} modbus_error_recovery_mode;


typedef void (*modbus_monitor_add_item_fnc_t)(modbus_t *ctx,
        uint8_t isOut, uint8_t slave, uint8_t func, uint16_t addr, uint16_t nb, 
        uint16_t expectedCRC, uint16_t actualCRC );
typedef void (*modbus_monitor_raw_data_fnc_t)(modbus_t *ctx,
        uint8_t *data, uint8_t dataLen, uint8_t addNewline,uint8_t);

MODBUS_API int modbus_set_slave(modbus_t *ctx, int slave);
MODBUS_API int modbus_set_error_recovery(modbus_t *ctx, modbus_error_recovery_mode error_recovery);
MODBUS_API int modbus_set_socket(modbus_t *ctx, int s);
MODBUS_API int modbus_get_socket(modbus_t *ctx);

MODBUS_API int modbus_get_response_timeout(modbus_t *ctx, uint32_t *to_sec, uint32_t *to_usec);
MODBUS_API int modbus_set_response_timeout(modbus_t *ctx, uint32_t to_sec, uint32_t to_usec);

MODBUS_API int modbus_get_byte_timeout(modbus_t *ctx, uint32_t *to_sec, uint32_t *to_usec);
MODBUS_API int modbus_set_byte_timeout(modbus_t *ctx, uint32_t to_sec, uint32_t to_usec);

MODBUS_API int modbus_get_header_length(modbus_t *ctx);

MODBUS_API int modbus_connect(modbus_t *ctx);
MODBUS_API void modbus_close(modbus_t *ctx);

MODBUS_API void modbus_free(modbus_t *ctx);

MODBUS_API int modbus_flush(modbus_t *ctx);
MODBUS_API int modbus_set_debug(modbus_t *ctx, int flag);

MODBUS_API const char *modbus_strerror(int errnum);

MODBUS_API int modbus_read_bits(modbus_t *ctx, int addr, int nb, uint8_t *dest);
MODBUS_API int modbus_read_input_bits(modbus_t *ctx, int addr, int nb, uint8_t *dest);
MODBUS_API int modbus_read_registers(modbus_t *ctx, int addr, int nb, uint16_t *dest);
MODBUS_API int modbus_read_input_registers(modbus_t *ctx, int addr, int nb, uint16_t *dest);
MODBUS_API int modbus_write_bit(modbus_t *ctx, int coil_addr, int status);
MODBUS_API int modbus_write_register(modbus_t *ctx, int reg_addr, int value);
MODBUS_API int modbus_write_bits(modbus_t *ctx, int addr, int nb, const uint8_t *data);
MODBUS_API int modbus_write_registers(modbus_t *ctx, int addr, int nb, const uint16_t *data);
MODBUS_API int modbus_mask_write_register(modbus_t *ctx, int addr, uint16_t and_mask, uint16_t or_mask);
MODBUS_API int modbus_write_and_read_registers(modbus_t *ctx, int write_addr, int write_nb,
                                               const uint16_t *src, int read_addr, int read_nb,
                                               uint16_t *dest);
MODBUS_API int modbus_report_slave_id(modbus_t *ctx, int max_dest, uint8_t *dest);

MODBUS_API modbus_mapping_t* modbus_mapping_new(int nb_bits, int nb_input_bits,
                                            int nb_registers, int nb_input_registers);
MODBUS_API void modbus_mapping_free(modbus_mapping_t *mb_mapping);

MODBUS_API int modbus_send_raw_request(modbus_t *ctx, uint8_t *raw_req, int raw_req_length);

MODBUS_API int modbus_receive(modbus_t *ctx, uint8_t *req);

MODBUS_API int modbus_receive_confirmation(modbus_t *ctx, uint8_t *rsp);

MODBUS_API int modbus_reply(modbus_t *ctx, const uint8_t *req,
                            int req_length, modbus_mapping_t *mb_mapping);
MODBUS_API int modbus_reply_exception(modbus_t *ctx, const uint8_t *req,
                                      unsigned int exception_code);
MODBUS_API void modbus_register_monitor_add_item_fnc(modbus_t *ctx,
                                                    modbus_monitor_add_item_fnc_t cb); 
MODBUS_API void modbus_register_monitor_raw_data_fnc(modbus_t *ctx,
                                                    modbus_monitor_raw_data_fnc_t cb); 

void modbus_poll(modbus_t *ctx);


/**
 * UTILS FUNCTIONS
 **/

#define MODBUS_GET_HIGH_BYTE(data) (((data) >> 8) & 0xFF)
#define MODBUS_GET_LOW_BYTE(data) ((data) & 0xFF)
#define MODBUS_GET_INT32_FROM_INT16(tab_int16, index) ((tab_int16[(index)] << 16) + tab_int16[(index) + 1])
#define MODBUS_GET_INT16_FROM_INT8(tab_int8, index) ((tab_int8[(index)] << 8) + tab_int8[(index) + 1])
#define MODBUS_SET_INT16_TO_INT8(tab_int8, index, value) \
    do { \
        tab_int8[(index)] = (value) >> 8;  \
        tab_int8[(index) + 1] = (value) & 0xFF; \
    } while (0)

MODBUS_API void modbus_set_bits_from_byte(uint8_t *dest, int idx, const uint8_t value);
MODBUS_API void modbus_set_bits_from_bytes(uint8_t *dest, int idx, unsigned int nb_bits,
                                       const uint8_t *tab_byte);
MODBUS_API uint8_t modbus_get_byte_from_bits(const uint8_t *src, int idx, unsigned int nb_bits);
MODBUS_API float modbus_get_float(const uint16_t *src);
MODBUS_API float modbus_get_float_dcba(const uint16_t *src);
MODBUS_API void modbus_set_float(float f, uint16_t *dest);
MODBUS_API void modbus_set_float_dcba(float f, uint16_t *dest);

MODBUS_END_DECLS

#endif  /* MODBUS_H */


#ifndef MODBUS_RTU_H
#define MODBUS_RTU_H


MODBUS_BEGIN_DECLS

/* Modbus_Application_Protocol_V1_1b.pdf Chapter 4 Section 1 Page 5
 * RS232 / RS485 ADU = 253 bytes + slave (1 byte) + CRC (2 bytes) = 256 bytes
 */
#define MODBUS_RTU_MAX_ADU_LENGTH  256

MODBUS_API modbus_t* modbus_new_rtu(const char *device, int baud, char parity,
                                int data_bit, int stop_bit);

#define MODBUS_RTU_RS232 0
#define MODBUS_RTU_RS485 1

MODBUS_API int modbus_rtu_set_serial_mode(modbus_t *ctx, int mode);
MODBUS_API int modbus_rtu_get_serial_mode(modbus_t *ctx);

#define MODBUS_RTU_RTS_NONE   0
#define MODBUS_RTU_RTS_UP     1
#define MODBUS_RTU_RTS_DOWN   2

MODBUS_API int modbus_rtu_set_rts(modbus_t *ctx, int mode);
MODBUS_API int modbus_rtu_get_rts(modbus_t *ctx);

MODBUS_END_DECLS

#endif /* MODBUS_RTU_H */



#ifndef MODBUS_TCP_H
#define MODBUS_TCP_H

MODBUS_BEGIN_DECLS

#if defined(_WIN32) && !defined(__CYGWIN__)
/* Win32 with MinGW, supplement to <errno.h> */
#include <winsock2.h>
#if !defined(ECONNRESET)
#define ECONNRESET   WSAECONNRESET
#endif
#if !defined(ECONNREFUSED)
#define ECONNREFUSED WSAECONNREFUSED
#endif
#if !defined(ETIMEDOUT)
#define ETIMEDOUT    WSAETIMEDOUT
#endif
#if !defined(ENOPROTOOPT)
#define ENOPROTOOPT  WSAENOPROTOOPT
#endif
#if !defined(EINPROGRESS)
#define EINPROGRESS  WSAEINPROGRESS
#endif
#endif

#define MODBUS_TCP_DEFAULT_PORT   502
#define MODBUS_TCP_SLAVE         0xFF

/* Modbus_Application_Protocol_V1_1b.pdf Chapter 4 Section 1 Page 5
 * TCP MODBUS ADU = 253 bytes + MBAP (7 bytes) = 260 bytes
 */
#define MODBUS_TCP_MAX_ADU_LENGTH  260

MODBUS_API modbus_t* modbus_new_tcp(const char *ip_address, int port);
MODBUS_API int modbus_tcp_listen(modbus_t *ctx, int nb_connection);
MODBUS_API int modbus_tcp_accept(modbus_t *ctx, int *s);

MODBUS_API modbus_t* modbus_new_tcp_pi(const char *node, const char *service);
MODBUS_API int modbus_tcp_pi_listen(modbus_t *ctx, int nb_connection);
MODBUS_API int modbus_tcp_pi_accept(modbus_t *ctx, int *s);

MODBUS_END_DECLS

#endif /* MODBUS_TCP_H */




#ifndef _MODBUS_ASCII_H_
#define _MODBUS_ASCII_H_


MODBUS_BEGIN_DECLS

/* Modbus_Application_Protocol_V1_1b.pdf Chapter 4 Section 1 Page 5
 * RS232 / RS485 ADU = 253 bytes + slave (1 byte) + CRC (2 bytes) = 256 bytes
 */
#define MODBUS_ASCII_MAX_ADU_LENGTH  256

MODBUS_API modbus_t* modbus_new_ascii(const char *device, int baud, char parity,
                                int data_bit, int stop_bit);

#define MODBUS_ASCII_RS232 0
#define MODBUS_ASCII_RS485 1

MODBUS_API int modbus_ascii_set_serial_mode(modbus_t *ctx, int mode);
MODBUS_API int modbus_ascii_get_serial_mode(modbus_t *ctx);

MODBUS_END_DECLS

#endif /* _MODBUS_ASCII_H_ */

#ifndef _MODBUS_ASCII_PRIVATE_H_
#define _MODBUS_ASCII_PRIVATE_H_

#ifndef _MSC_VER
#include <stdint.h>
#else
#include "stdint.h"
#endif

#if defined(_WIN32)
#include <windows.h>
#else
#include <termios.h>
#endif

#define _MODBUS_ASCII_HEADER_LENGTH      2
#define _MODBUS_ASCII_PRESET_REQ_LENGTH  7
#define _MODBUS_ASCII_PRESET_RSP_LENGTH  2

#define _MODBUS_ASCII_CHECKSUM_LENGTH    3 /* lcr8 + \r\n */

#if defined(_WIN32)
#if !defined(ENOTSUP)
#define ENOTSUP WSAEOPNOTSUPP
#endif

/* WIN32: struct containing serial handle and a receive buffer */
#define PY_BUF_SIZE 512
struct win32_ser {
    /* File handle */
    HANDLE fd;
    /* Receive buffer */
    uint8_t buf[PY_BUF_SIZE];
    /* Received chars */
    DWORD n_bytes;
};
#endif /* _WIN32 */

typedef struct _modbus_rtu {
    /* Device: "/dev/ttyS0", "/dev/ttyUSB0" or "/dev/tty.USA19*" on Mac OS X. */
    char *device;
    /* Bauds: 9600, 19200, 57600, 115200, etc */
    int baud;
    /* Data bit */
    uint8_t data_bit;
    /* Stop bit */
    uint8_t stop_bit;
    /* Parity: 'N', 'O', 'E' */
    char parity;
#if defined(_WIN32)
    struct win32_ser w_ser;
    DCB old_dcb;
#else
    /* Save old termios settings */
    struct termios old_tios;
#endif
/* #if HAVE_DECL_TIOCSRS485 */
    int serial_mode;
/* #endif */
#if HAVE_DECL_TIOCM_RTS
    int rts;
    int onebyte_time;
#endif
    /* To handle many slaves on the same link */
    int confirmation_to_ignore;
} modbus_ascii_t;

#endif /* _MODBUS_RTU_PRIVATE_H_ */

#ifndef MODBUS_PRIVATE_H
#define MODBUS_PRIVATE_H

#ifndef _MSC_VER
# include <stdint.h>
# include <sys/time.h>
#else
# include "stdint.h"
# include <time.h>
typedef int ssize_t;
#endif
#include <sys/types.h>
#include <config.h>

MODBUS_BEGIN_DECLS

/* It's not really the minimal length (the real one is report slave ID
 * in RTU (4 bytes)) but it's a convenient size to use in RTU or TCP
 * communications to read many values or write a single one.
 * Maximum between :
 * - HEADER_LENGTH_TCP (7) + function (1) + address (2) + number (2)
 * - HEADER_LENGTH_RTU (1) + function (1) + address (2) + number (2) + CRC (2)
 */
#define _MIN_REQ_LENGTH 12

#define _REPORT_SLAVE_ID 180

#define _MODBUS_EXCEPTION_RSP_LENGTH 5

/* Timeouts in microsecond (0.5 s) */
#define _RESPONSE_TIMEOUT    500000
#define _BYTE_TIMEOUT        500000

typedef enum {
    _MODBUS_BACKEND_TYPE_RTU=0,
    _MODBUS_BACKEND_TYPE_TCP, 
    _MODBUS_BACKEND_TYPE_ASCII
} modbus_backend_type_t;

/*
 *  ---------- Request     Indication ----------
 *  | Client | ---------------------->| Server |
 *  ---------- Confirmation  Response ----------
 */
typedef enum {
    /* Request message on the server side */
    MSG_INDICATION,
    /* Request message on the client side */
    MSG_CONFIRMATION
} msg_type_t;

/* This structure reduces the number of params in functions and so
 * optimizes the speed of execution (~ 37%). */
typedef struct _sft {
    int slave;
    int function;
    int t_id;
} sft_t;


typedef struct _modbus_backend {
    unsigned int backend_type;
    unsigned int header_length;
    unsigned int checksum_length;
    unsigned int max_adu_length;
    int (*set_slave) (modbus_t *ctx, int slave);
    int (*build_request_basis) (modbus_t *ctx, int function, int addr,
                                int nb, uint8_t *req);
    int (*build_response_basis) (sft_t *sft, uint8_t *rsp);
    int (*prepare_response_tid) (const uint8_t *req, int *req_length);
    int (*send_msg_pre) (uint8_t *req, int req_length);
    ssize_t (*send) (modbus_t *ctx, const uint8_t *req, int req_length);
    int (*receive) (modbus_t *ctx, uint8_t *req);
    ssize_t (*recv) (modbus_t *ctx, uint8_t *rsp, int rsp_length);
    int (*check_integrity) (modbus_t *ctx, uint8_t *msg,
                            const int msg_length);
    int (*pre_check_confirmation) (modbus_t *ctx, const uint8_t *req,
                                   const uint8_t *rsp, int rsp_length);
    int (*connect) (modbus_t *ctx);
    void (*close) (modbus_t *ctx);
    int (*flush) (modbus_t *ctx);
    int (*select) (modbus_t *ctx, fd_set *rset, struct timeval *tv, int msg_length);
    void (*free) (modbus_t *ctx);
} modbus_backend_t;

struct _modbus {
    /* Slave address */
    int slave;
    /* Socket or file descriptor */
    int s;
    int debug;
    int error_recovery;
    struct timeval response_timeout;
    struct timeval byte_timeout;
  uint16_t last_crc_expected;
  uint16_t last_crc_received;
    const modbus_backend_t *backend;
    void *backend_data;
    modbus_monitor_add_item_fnc_t monitor_add_item;
    modbus_monitor_raw_data_fnc_t monitor_raw_data;
};

void _modbus_init_common(modbus_t *ctx);
void _error_print(modbus_t *ctx, const char *context);
int _modbus_receive_msg(modbus_t *ctx, uint8_t *msg, msg_type_t msg_type);

#ifndef HAVE_STRLCPY
size_t strlcpy(char *dest, const char *src, size_t dest_size);
#endif

MODBUS_END_DECLS

#endif  /* MODBUS_PRIVATE_H */
