#ifndef GENERIC_GPIO_LIBGPIOD_H
#define GENERIC_GPIO_LIBGPIOD_H

#define DRIVER_NAME	"GPIO UPS driver"
#define DRIVER_VERSION	"1.00"

extern struct gpioups_t *gpioupsfd;

typedef struct libgpiod_data_t {
    struct gpiod_chip *gpioChipHandle;      /* libgpiod chip handle when opened */
    struct gpiod_line_bulk gpioLines;       /* libgpiod lines to monitor */
    struct gpiod_line_bulk gpioEventLines;  /* libgpiod lines for event monitoring */
} libgpiod_data;

#endif	/* GENERIC_GPIO_LIBGPIOD_H */
