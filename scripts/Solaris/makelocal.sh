#!/bin/sh

# Creates the package file from current-directory contents
# Called by Makefile starting from installed prototype directory

echo "Making Solaris SVR4 package metadata..." && \
pkgmk -o -d "`pwd`" && \
echo "Making Solaris SVR4 package archive file..." && \
( yes "" | pkgtrans "`pwd`" "`pwd`/NUT_solaris_package.local" ) && \
echo "Compressing Solaris SVR4 package archive file..." && \
gzip "`pwd`/NUT_solaris_package.local"
