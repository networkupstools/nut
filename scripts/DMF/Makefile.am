RM ?= /usr/bin/rm -f
# Take compiler from PATH, may be wrapped with ccache etc.
CC = gcc
CPP = gcc
CXX = g++

PROGS = dmf-test dmf-lua-test

# First target is default
all: $(PROGS)

clean:
	$(RM) $(PROGS)

check: run-dmf-test run-dmf-lua-test

dmf-test: dmf.c
	$(CC) -ggdb -std=c11 -std=gnu99 -Werror -Wall -pedantic -Wc++-compat \
	-D_FORTIFY_SOURCE=2 -O -fstack-protector \
	-lneon -I../../include -I../../drivers \
	-DWITH_DMFTEST_MAIN=1 -DDEBUG=1 \
	-o $@ $^

dmf-lua-test: dmf.c
	$(CC) -ggdb -std=c11 -std=gnu99 -Werror -Wall -pedantic -Wc++-compat \
	-D_FORTIFY_SOURCE=2 -O -fstack-protector \
	-lneon -I../../include -I../../drivers \
	-llua5.1 -I/usr/include/lua5.1 \
	-DWITH_DMFTEST_MAIN=1 -DDEBUG=1 \
	-DWITH_DMF_LUA=1 \
	-o $@ $^

run-dmf-test: dmf-test
	valgrind --leak-check=full $<

run-dmf-lua-test: dmf-lua-test
	valgrind --leak-check=full --track-origins=yes $<
