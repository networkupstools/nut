#!/bin/sh

pkgmk -o -d `pwd` 
pkgtrans `pwd` `pwd`/NUT_solaris_package.local
gzip `pwd`/NUT_solaris_package.local
