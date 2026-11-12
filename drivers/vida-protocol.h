/* vida-protocol.h - closed telemetry contract for VIDA Armour USB UPSes.
 * All data is validated before the driver publishes a complete sample.
 * Copyright (C) 2026 NUT contributors. GPL version 2 or later.
 */
#ifndef NUT_VIDA_PROTOCOL_H
#define NUT_VIDA_PROTOCOL_H

#include <stddef.h>
#include "nut_stdint.h"

#define VIDA_TRANSFER_LENGTH 102
#define VIDA_TEXT_LENGTH 51

typedef enum {
	VIDA_STATUS,
	VIDA_RATINGS,
	VIDA_CHARGE,
	VIDA_FIRMWARE,
	VIDA_QUERY_COUNT
} vida_query_t;

typedef struct {
	double input_voltage;
	double fault_voltage;
	double output_voltage;
	double load;
	double frequency;
	double battery_voltage;
	double temperature;
	double charge;
	double nominal_voltage;
	double nominal_current;
	double nominal_battery_voltage;
	double nominal_frequency;
	char flags[9];
	char firmware[32];
} vida_sample_t;

/* Only these four enum values can produce a USB string descriptor request. */
int vida_query_value(vida_query_t query, uint16_t *value);
/* Accept only complete, ASCII UTF-16LE string descriptors ending in CR. */
int vida_decode_string(const unsigned char *raw, size_t length,
	char *text, size_t capacity);
/* Parse one record into a not-yet-published sample. Zero means invalid. */
int vida_parse_record(vida_query_t query, const char *text, vida_sample_t *sample);

#endif /* NUT_VIDA_PROTOCOL_H */
