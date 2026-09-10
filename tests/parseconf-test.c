/* parseconf-test - configuration and protocol token boundaries

   Copyright (C) 2026 Network UPS Tools project

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#include "config.h" /* should be first */
#include "common.h"
#include "parseconf.h"

typedef struct {
	const char *name;
	const char *input;
	size_t numargs;
	const char *args[8];
} testcase_t;

static const testcase_t cases[] = {
	{ "ordinary", "one two\n", 2, { "one", "two" } },
	{ "empty", " \t\n", 0, { NULL } },
	{ "comment", "# ignored\n", 0, { NULL } },
	{ "unquoted hash", "Outlet #1\n", 1, { "Outlet" } },
	{ "attached comment", "my#1ups\n", 1, { "my" } },
	{ "quoted hash", "\"Outlet #3\"\n", 1, { "Outlet #3" } },
	{ "quoted hash boundaries", "\"#\" \"##last#\"\n", 2, { "#", "##last#" } },
	{ "escaped hash", "\"Outlet \\#2\" Outlet \\#4\n", 3,
		{ "Outlet #2", "Outlet", "#4" } },
	{ "comment after quote", "\"# value\"# ignored\n", 1, { "# value" } },
	{ "empty quotes", "\"\" \"\"\n", 2, { "", "" } },
	{ "closing quote commits", "\"one\"two \"three\"\"four\"\n", 4,
		{ "one", "two", "three", "four" } },
	{ "quote inside unquoted word", "a\"b\"c\n", 1, { "a\"b\"c" } },
	{ "apostrophes", "it's 'two words' \"'literal'\"\n", 4,
		{ "it's", "'two", "words'", "'literal'" } },
	{ "equal separators", "a=b==\"c=d\"\n", 6, { "a", "=", "b", "=", "=", "c=d" } },
	{ "escaped spaces and equal", "one\\ two \\=value \"a\\ b\\=c\"\n", 3,
		{ "one two", "=value", "a b=c" } },
	{ "escaped letters", "\\n\\t\\r\\x41 \"\\n\\t\\r\\x41\"\n", 2,
		{ "ntrx41", "ntrx41" } },
	{ "escaped quotes", "\\\"edge\\\" \"\\\"edge\\\"\"\n", 2,
		{ "\"edge\"", "\"edge\"" } },
	{ "escaped backslashes", "a\\\\b \"a\\\\b\\\\\\\\c\"\n", 2,
		{ "a\\b", "a\\b\\\\c" } },
	{ "backslash before hash", "\"a\\\\#b\" \"a\\\\\\#b\"\n", 2,
		{ "a\\#b", "a\\#b" } },
	{ "backslash before quote", "\"a\\\\\"b \"a\\\\\\\"b\"\n", 3,
		{ "a\\", "b", "a\\\"b" } },
	{ "continuation", "one\\\ntwo \"three\\\nfour\"\n", 2,
		{ "onetwo", "threefour" } },
	{ "quoted physical newline", "\"one\ntwo\"\n", 1, { "onetwo" } },
	{ "character filter", "\"a\001\t\r\200\377b\177\"\n", 1, { "ab\177" } }
};

static int failures = 0;

static void check_args(PCONF_CTX_t *ctx, const testcase_t *test, const char *mode)
{
	size_t i;

	if (pconf_parse_error(ctx)) {
		fprintf(stderr, "%s/%s: unexpected parse error: %s\n",
			test->name, mode, ctx->errmsg);
		failures++;
	}
	if (ctx->numargs != test->numargs) {
		fprintf(stderr, "%s/%s: got %" PRIuSIZE " arguments, expected %" PRIuSIZE "\n",
			test->name, mode, ctx->numargs, test->numargs);
		failures++;
		return;
	}
	for (i = 0; i < test->numargs; i++) {
		if (strcmp(ctx->arglist[i], test->args[i])) {
			fprintf(stderr, "%s/%s: argument %" PRIuSIZE ": got <%s>, expected <%s>\n",
				test->name, mode, i, ctx->arglist[i], test->args[i]);
			failures++;
		}
	}
}

static void check_result(int actual, int expected, const char *name)
{
	if (actual != expected) {
		fprintf(stderr, "%s: got %d, expected %d\n", name, actual, expected);
		failures++;
	}
}

static void file_input(PCONF_CTX_t *ctx, const char *input)
{
	/* tmpfile() gives each run a private stream and removes it on close. */
	ctx->f = tmpfile();
	if (!ctx->f)
		fatal_with_errno(EXIT_FAILURE, "tmpfile");
	if (fputs(input, ctx->f) == EOF || fflush(ctx->f))
		fatal_with_errno(EXIT_FAILURE, "write test input");
	rewind(ctx->f);
}

static void check_case(const testcase_t *test)
{
	PCONF_CTX_t ctx;
	size_t i, len = strlen(test->input);
	int ret;

	pconf_init(&ctx, NULL);
	check_result(pconf_line(&ctx, test->input), 1, test->name);
	check_args(&ctx, test, "line");
	pconf_finish(&ctx);

	pconf_init(&ctx, NULL);
	file_input(&ctx, test->input);
	check_result(pconf_file_next(&ctx), 1, test->name);
	check_args(&ctx, test, "file");
	check_result(pconf_file_next(&ctx), 0, "file EOF");
	pconf_finish(&ctx);

	pconf_init(&ctx, NULL);
	for (i = 0; i < len; i++) {
		ret = pconf_char(&ctx, test->input[i]);
		check_result(ret, (i == len - 1) ? 1 : 0, test->name);
		if (ret < 0)
			break;
	}
	check_args(&ctx, test, "char");
	pconf_finish(&ctx);
}

static void check_boundaries(void)
{
	PCONF_CTX_t ctx;
	static const testcase_t incomplete = {
		"incomplete quote", "key \"unfinished", 2, { "key", "unfinished" }
	};
	static const testcase_t limited = {
		"limits", "abcde fghij discarded\n", 2, { "abcd", "fghi" }
	};
	static const testcase_t trailing = {
		"trailing backslash", "key value\\", 2, { "key", "value" }
	};
	static const testcase_t next = {
		"next line", "next\n", 1, { "next" }
	};
	size_t i;

	/* These incomplete inputs historically return their collected words. */
	pconf_init(&ctx, NULL);
	check_result(pconf_line(&ctx, incomplete.input), 1, incomplete.name);
	check_args(&ctx, &incomplete, "line");
	check_result(pconf_line(&ctx, trailing.input), 1, trailing.name);
	check_args(&ctx, &trailing, "line");
	ctx.arg_limit = 2;
	ctx.wordlen_limit = 4;
	check_result(pconf_line(&ctx, limited.input), 1, limited.name);
	check_args(&ctx, &limited, "line");
	pconf_finish(&ctx);

	pconf_init(&ctx, NULL);
	file_input(&ctx, incomplete.input);
	check_result(pconf_file_next(&ctx), 1, incomplete.name);
	check_args(&ctx, &incomplete, "file");
	check_result(pconf_file_next(&ctx), 0, "incomplete file EOF");
	pconf_finish(&ctx);

	pconf_init(&ctx, NULL);
	for (i = 0; i < strlen(incomplete.input); i++)
		check_result(pconf_char(&ctx, incomplete.input[i]), 0, "incomplete char input");
	check_result(pconf_parse_error(&ctx), 0, "incomplete char error");
	pconf_finish(&ctx);

	pconf_init(&ctx, NULL);
	for (i = 0; i < 2; i++) {
		check_result(pconf_char(&ctx, '"'), 0, "empty token start");
		check_result(pconf_char(&ctx, '"'), 0, "empty token end");
		check_result(pconf_char(&ctx, '\n'), 1, "char line end");
		check_result(pconf_line(&ctx, next.input), 1, "next line");
		check_args(&ctx, &next, "reused context");
	}
	pconf_finish(&ctx);
	check_result(pconf_line(&ctx, "ignored"), 0, "finished context");
	check_result(pconf_line(NULL, "ignored"), 0, "NULL context");
	check_result(pconf_char(NULL, 'x'), -1, "NULL char context");
}

static void check_encoder(void)
{
	static const char raw[] = "# \\\"' = end#";
	static const char encoded[] = "\\# \\\\\\\"' = end\\#";
	PCONF_CTX_t ctx;
	char output[128], input[132];
	const testcase_t roundtrip = { "encoder roundtrip", NULL, 1, { raw } };
	const testcase_t short_result = { "short encoder roundtrip", NULL, 1, { "#" } };

	check_result(pconf_encode(raw, output, sizeof(output)) == output, 1, "encoder return");
	check_result(strcmp(output, encoded), 0, "encoder bytes (including escaped hash)");
	snprintf(input, sizeof(input), "\"%s\"\n", output);
	pconf_init(&ctx, NULL);
	check_result(pconf_line(&ctx, input), 1, "encoder parse");
	check_args(&ctx, &roundtrip, "line");

	/* A truncated encoding must not leave an unmatched escape character. */
	pconf_encode("##", output, 4);
	check_result(strcmp(output, "\\#"), 0, "short encoder bytes");
	snprintf(input, sizeof(input), "\"%s\"\n", output);
	check_result(pconf_line(&ctx, input), 1, "short encoder parse");
	check_args(&ctx, &short_result, "line");
	pconf_encode("#", output, 2);
	check_result(output[0], '\0', "encoder cannot fit escape pair");
	output[0] = 'x';
	pconf_encode("#", output, 0);
	check_result(output[0], 'x', "zero sized encoder destination");
	pconf_finish(&ctx);
}

int main(void)
{
	size_t i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
		check_case(&cases[i]);
	check_boundaries();
	check_encoder();
	printf("parseconf: %" PRIuSIZE " cases through line/file/char, boundary and encoder checks: %d failures\n",
		sizeof(cases) / sizeof(cases[0]), failures);
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
