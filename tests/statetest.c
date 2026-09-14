/* statetest.c - test value updates in the common NUT state store
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "common.h"
#include "state.h"

static int failures;

static void check(int condition, const char *message)
{
	if (!condition) {
		upslogx(LOG_ERR, "%s", message);
		failures++;
	}
}

static size_t node_count(const st_tree_t *node)
{
	if (!node) {
		return 0;
	}
	return 1 + node_count(node->left) + node_count(node->right);
}

static void update(st_tree_t **root, const char *name, const char *value,
	int changed, const char *raw, const char *escaped)
{
	st_tree_t *before = state_tree_find(*root, name), *node;
	st_tree_timespec_t start, finish;
	int flags = before ? before->flags : 0;
	long aux = before ? before->aux : 0;
	const enum_t *enums = before ? before->enum_list : NULL;
	const range_t *ranges = before ? before->range_list : NULL;
	size_t count = node_count(*root);
	int actual;

	/* A sentinel proves refresh even when consecutive clock reads tie. */
	if (before) {
		memset(&before->lastset, 0, sizeof(before->lastset));
	}
	if (state_get_timestamp(&start) != 0) {
		fatal_with_errno(EXIT_FAILURE, "read start time");
	}
	actual = state_setinfo(root, name, value);
	if (state_get_timestamp(&finish) != 0) {
		fatal_with_errno(EXIT_FAILURE, "read finish time");
	}
	node = state_tree_find(*root, name);
	upslogx(LOG_INFO, "%s: input='%s', changed=%d, stored='%s'",
		name, value, actual, node ? node->raw : "<missing>");
	check(actual == changed, "unexpected change result");
	check(node != NULL, "updated node missing");
	if (!node) {
		return;
	}
	check(!before || node == before, "existing node replaced");
	check(node_count(*root) == count + (before ? 0 : 1), "tree size changed");
	check(!strcmp(node->raw, raw), "raw value differs");
	check(!strcmp(state_getinfo(*root, name), escaped), "escaped value differs");
	check(node->rawsize >= strlen(node->raw) + 1, "raw capacity too small");
	if (!strcmp(raw, escaped)) {
		check(node->val == node->raw, "plain value does not use raw storage");
	} else {
		check(node->val == node->safe, "escaped value does not use safe storage");
		check(node->safesize >= strlen(node->val) + 1, "escaped capacity too small");
	}
	check(node->flags == flags && node->aux == aux, "flags or aux changed");
	check(node->enum_list == enums && node->range_list == ranges, "metadata lists changed");
	check(st_tree_node_compare_timestamp(node, &start) >= 0, "timestamp not refreshed");
	check(st_tree_node_compare_timestamp(node, &finish) <= 0, "timestamp after call");
}

static void check_strings(size_t numflags, char **flags, int expected_flags)
{
	st_tree_t *root = NULL;
	st_tree_t *node;
	const enum_t *enums;
	const range_t *ranges;

	update(&root, "device.contact", "Operations", 1, "Operations", "Operations");
	state_setflags(root, "device.contact", numflags, flags);
	check(state_getflags(root, "device.contact") == expected_flags, "set string flags");
	check(state_setaux(root, "device.contact", "64") == 1, "set aux");
	update(&root, "battery.type", "Lead", 1, "Lead", "Lead");
	update(&root, "ups.model", "Model", 1, "Model", "Model");
	update(&root, "battery.charge", "100", 1, "100", "100");
	update(&root, "ups.serial", "Serial", 1, "Serial", "Serial");

	update(&root, "DEVICE.CONTACT", "Operations", 0, "Operations", "Operations");
	update(&root, "device.contact", "operations", 1, "operations", "operations");
	update(&root, "device.contact", "operations", 0, "operations", "operations");
	/* An ordinary change makes the reverse case test independent. */
	update(&root, "device.contact", "Engineering", 1, "Engineering", "Engineering");
	update(&root, "device.contact", "operations", 1, "operations", "operations");
	update(&root, "Device.Contact", "Operations", 1, "Operations", "Operations");
	update(&root, "device.contact", "Operations", 0, "Operations", "Operations");
	update(&root, "device.contact", "Production", 1, "Production", "Production");
	update(&root, "device.contact", "Ops", 1, "Ops", "Ops");
	update(&root, "device.contact", "Operations department", 1,
		"Operations department", "Operations department");
	update(&root, "device.contact", "", 1, "", "");
	update(&root, "device.contact", "", 0, "", "");
	update(&root, "device.contact", "Ops \"A\"\\Desk", 1,
		"Ops \"A\"\\Desk", "Ops \\\"A\\\"\\\\Desk");
	update(&root, "device.contact", "ops \"a\"\\desk", 1,
		"ops \"a\"\\desk", "ops \\\"a\\\"\\\\desk");
	update(&root, "device.contact", "A longer \"quoted\"\\value", 1,
		"A longer \"quoted\"\\value", "A longer \\\"quoted\\\"\\\\value");
	update(&root, "device.contact", "Plain", 1, "Plain", "Plain");

	update(&root, "BATTERY.TYPE", "lead", 1, "lead", "lead");
	update(&root, "UPS.MODEL", "model", 1, "model", "model");
	update(&root, "Ups.Serial", "serial", 1, "serial", "serial");
	check(!strcmp(state_getinfo(root, "device.contact"), "Plain"), "root corrupted");
	check(!strcmp(state_getinfo(root, "battery.charge"), "100"), "sibling corrupted");
	check(node_count(root) == 5, "name matching created duplicate nodes");
	node = state_tree_find(root, "DEVICE.CONTACT");
	if (!node) {
		fatalx(EXIT_FAILURE, "device.contact node missing");
	}
	check(!strcmp(node->var, "device.contact"), "stored variable name changed");

	/* The internal immutable bit is set by driver override handling;
	 * state_setflags() does not receive this bit over the socket protocol. */
	node->flags |= ST_FLAG_IMMUTABLE;
	update(&root, "device.contact", "plain", 0, "Plain", "Plain");
	update(&root, "DEVICE.CONTACT", "Other", 0, "Plain", "Plain");
	update(&root, "device.contact", "Plain", 0, "Plain", "Plain");
	node->flags &= ~ST_FLAG_IMMUTABLE;
	update(&root, "device.contact", "\"A\"", 1, "\"A\"", "\\\"A\\\"");
	node->flags |= ST_FLAG_IMMUTABLE;
	update(&root, "device.contact", "\"a\"", 0, "\"A\"", "\\\"A\\\"");
	update(&root, "device.contact", "Other", 0, "\"A\"", "\\\"A\\\"");

	update(&root, "input.sensitivity", "Normal", 1, "Normal", "Normal");
	check(state_addenum(root, "input.sensitivity", "Normal") == 1, "add enum");
	check(state_addenum(root, "input.sensitivity", "Reduced") == 1, "add second enum");
	enums = state_getenumlist(root, "input.sensitivity");
	update(&root, "INPUT.SENSITIVITY", "Reduced", 1, "Reduced", "Reduced");
	check(enums && !strcmp(enums->val, "Normal") && enums->next
		&& !strcmp(enums->next->val, "Reduced"), "enum content changed");
	check(state_addrange(root, "battery.charge", 0, 100) == 1, "add range");
	ranges = state_getrangelist(root, "battery.charge");
	update(&root, "BATTERY.CHARGE", "90", 1, "90", "90");
	check(ranges && ranges->min == 0 && ranges->max == 100, "range content changed");
	state_infofree(root);
}

int main(void)
{
	char *flags[] = { "STRING", "RW" };
	char *number[] = { "NUMBER" };
	st_tree_t *root = NULL;

	check_strings(0, NULL, 0);
	check_strings(1, flags, ST_FLAG_STRING);
	check_strings(1, flags + 1, ST_FLAG_RW);
	check_strings(2, flags, ST_FLAG_STRING | ST_FLAG_RW);
	update(&root, "input.voltage", "120", 1, "120", "120");
	state_setflags(root, "input.voltage", 1, number);
	check(state_getflags(root, "input.voltage") == ST_FLAG_NUMBER, "set number flag");
	update(&root, "input.voltage", "121", 1, "121", "121");
	update(&root, "input.voltage", "9", 1, "9", "9");
	update(&root, "input.voltage", "01200.2", 1, "01200.2", "01200.2");
	update(&root, "input.voltage", "1200.20", 1, "1200.20", "1200.20");
	update(&root, "input.voltage", "1200.20", 0, "1200.20", "1200.20");
	state_infofree(root);
	upslogx(LOG_INFO, "State update tests: %d failures", failures);
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
