/* nutrandomtest - checks for nut_random_bytes()

   Copyright (C) 2026 Network UPS Tools project

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#include "common.h"

static int failures;

static void check(int condition, const char *message)
{
	if (!condition) {
		printf("FAIL: %s\n", message);
		failures++;
	} else {
		printf("PASS: %s\n", message);
	}
}

int main(void)
{
	unsigned char	a[64], b[64], big[4096], guard[8];
	unsigned int	seen_lo = 0, seen_hi = 0;
	size_t	i;

	/* zero length must not touch the buffer */
	memset(guard, 0x5A, sizeof(guard));
	nut_random_bytes(guard, 0);
	for (i = 0; i < sizeof(guard) && guard[i] == 0x5A; i++)
		;
	check(i == sizeof(guard), "zero length leaves the buffer alone");

	/* two draws in a row differ (fails with chance 2^-512) */
	nut_random_bytes(a, sizeof(a));
	nut_random_bytes(b, sizeof(b));
	check(memcmp(a, b, sizeof(a)) != 0, "consecutive draws differ");

	/* both nibbles take every value over 4096 bytes: catches a constant
	 * or a generator stuck in a few low or high bits (a miss has a chance
	 * of about 16 * (15/16)^4096, i.e. never) */
	nut_random_bytes(big, sizeof(big));
	for (i = 0; i < sizeof(big); i++) {
		seen_lo |= 1U << (big[i] & 0x0F);
		seen_hi |= 1U << (big[i] >> 4);
	}
	check(seen_lo == 0xFFFFU, "low nibbles take all 16 values");
	check(seen_hi == 0xFFFFU, "high nibbles take all 16 values");

	printf("nutrandomtest: %d failures\n", failures);
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
