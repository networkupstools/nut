# GNU Make syntax assumed below; fix this to be portable to other make's

# This file exists for developers to be able to run quickly without an
# "automade" project using e.g. `gmake -f GNUMakefile-local.mk dmf`.
# Note: then `bld=src=dir_of_makefile` is assumed below

RM ?= /usr/bin/rm -f
MKDIR_P ?= /usr/bin/mkdir -p
LN_S ?= /usr/bin/ln -s
# Take compiler from PATH, may be wrapped with ccache etc.
CC ?= gcc
CPP ?= gcc
CXX ?= g++

XMLLINT ?= xmllint
VALGRIND ?= valgrind

mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
current_dir := $(patsubst %/,%,$(dir $(mkfile_path)))

srcdir ?= .
builddir ?= .
top_srcdir ?= ../..
top_builddir ?= ../..
abs_srcdir ?= $(current_dir)
abs_builddir ?= $(current_dir)
abs_top_srcdir ?= $(current_dir)/../..
abs_top_builddir ?= $(current_dir)/../..

include Makefile.am

