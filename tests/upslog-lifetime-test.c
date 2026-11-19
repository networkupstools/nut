/* upslog-lifetime-test - owned system fields without a server or real signal
 *
 * Copyright (C) 2026 Network UPS Tools project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "common.h"
#include "upsclient.h"

static void *tracked_calloc(size_t count, size_t size);
static char *tracked_strdup(const char *value);
static void tracked_free(void *pointer);
static int tracked_disconnect(UPSCONN_t *connection);
static int tracked_fclose(FILE *stream);
int upslog_test_main(int argc, char **argv);

#define main upslog_test_main
#define xcalloc tracked_calloc
#define xstrdup tracked_strdup
#define free tracked_free
#define upscli_disconnect tracked_disconnect
#define fclose tracked_fclose
#include "upslog.c"
#undef main
#undef xcalloc
#undef xstrdup
#undef free
#undef upscli_disconnect
#undef fclose

static struct {
	void	*pointer;
	int	live;
} allocations[128];
static size_t allocation_count = 0;
static int failures = 0, checks = 0, disconnects = 0;
static FILE *borrowed_file = NULL;
static struct monhost_ups_t *disconnecting_node = NULL;

static void check(int condition, const char *label)
{
	checks++;
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", label);
		failures++;
	}
}

static void *remember(void *pointer)
{
	if (allocation_count >= sizeof(allocations) / sizeof(allocations[0])) {
		fatalx(EXIT_FAILURE, "Allocation fixture is full");
	}
	allocations[allocation_count].pointer = pointer;
	allocations[allocation_count].live = 1;
	allocation_count++;
	return pointer;
}

static int is_live(const void *pointer)
{
	size_t i;

	for (i = 0; i < allocation_count; i++) {
		if (allocations[i].pointer == pointer && allocations[i].live)
			return 1;
	}
	return 0;
}

static void *tracked_calloc(size_t count, size_t size)
{
	return remember(xcalloc(count, size));
}

static char *tracked_strdup(const char *value)
{
	return (char *)remember(xstrdup(value));
}

static void tracked_free(void *pointer)
{
	size_t i;

	if (!pointer)
		return;
	for (i = 0; i < allocation_count; i++) {
		if (allocations[i].pointer == pointer && allocations[i].live) {
			allocations[i].live = 0;
			free(pointer);
			return;
		}
	}
	check(0, "free only a live owned allocation once");
}

static int tracked_disconnect(UPSCONN_t *connection)
{
	disconnects++;
	check(is_live(connection), "connection is owned at disconnect");
	if (disconnecting_node) {
		check(is_live(disconnecting_node->monhost), "disconnect precedes string release");
	}
	return upscli_disconnect(connection);
}

static int tracked_fclose(FILE *stream)
{
	if (stream == borrowed_file || stream == stdout) {
		check(0, "system does not close a borrowed logfile or stdout");
		return EOF;
	}
	return fclose(stream);
}

int main(void)
{
	struct logtarget_t target;
	struct monhost_ups_t *first, *second, *discovered, *head = NULL, *p, *next;
	size_t count = 0, i;
	int state;

	memset(&target, 0, sizeof(target));
	borrowed_file = tmpfile();
	if (!borrowed_file)
		fatal_with_errno(EXIT_FAILURE, "tmpfile");
	target.logfile = borrowed_file;
	first = new_monhost("first", &target);
	check(first->ups == NULL && first->upsname == NULL && first->hostname == NULL
		&& first->port == 0 && first->next == NULL, "constructor initialises partial state");
	check(first->logtarget == &target && !strcmp(first->monhost, "first"), "target is borrowed and system copied");
	insert_monhost(&head, first, &count);
	second = new_monhost("second", &target);
	insert_monhost(&first->next, second, &count);
	first->upsname = tracked_strdup("*");
	first->hostname = tracked_strdup("localhost");
	first->port = 3493;
	discovered = new_discovered_monhost(first, "alpha");
	check(!strcmp(discovered->monhost, "alpha@localhost:3493")
		&& !strcmp(discovered->upsname, "alpha") && !strcmp(discovered->hostname, "localhost")
		&& discovered->hostname != first->hostname && discovered->upsname != first->upsname
		&& discovered->port == first->port && discovered->logtarget == &target,
		"discovery copies owned fields and borrows target");
	insert_monhost(&first->next, discovered, &count);
	check(head == first && first->next == discovered && discovered->next == second
		&& second->next == NULL && count == 3, "insert preserves successor and count");
	p = new_monhost("new-head", &target);
	insert_monhost(&head, p, &count);
	check(head == p && p->next == first && count == 4, "insert at head");
	for (p = head; p; p = next) {
		next = p->next;
		destroy_monhost(p, 0);
	}
	check(disconnects == 0, "partial nodes have no connection to disconnect");
	for (state = 0; state < 2; state++) {
		p = new_monhost("connection", &target);
		p->ups = (UPSCONN_t *)tracked_calloc(1, sizeof(*p->ups));
		check(upscli_connect(p->ups, NULL, 0, 0) < 0, "real no-network connection initialisation");
		if (state)
			upscli_disconnect(p->ups);
		disconnecting_node = p;
		destroy_monhost(p, 1);
		disconnecting_node = NULL;
	}
	check(disconnects == 2, "initialised and already-disconnected nodes each disconnected at teardown");
	check(fputs("borrowed target remains open\n", borrowed_file) >= 0, "borrowed file survives destruction");
	target.logfile = stdout;
	p = new_monhost("stdout", &target);
	destroy_monhost(p, 0);
	check(fputs("stdout remains open\n", stdout) >= 0 && fflush(stdout) == 0, "stdout survives destruction");
	for (i = 0; i < allocation_count; i++)
		check(!allocations[i].live, "owned allocation released");
	fclose(borrowed_file);
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
