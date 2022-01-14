#!/bin/bash

. ./version.sh

rm -Rf package
mkdir package

svn up

NAME=ipp-solaris-$IPP_VERSION.sparc
mkdir package/$NAME
FILE_LIST=`find . -type f -name "*" | grep -v ".svn" | grep -v "./nutconf-dummy" | grep -v "./make_package.sh" | grep -v "nut/" | grep -v install.log | grep -v uninstall.log | grep -v "package/" | grep -v aix | grep -v hpux | grep -v solint`
cp --parents -r $FILE_LIST package/$NAME
cd package
tar cpvf $NAME.tar $NAME
gzip -9 $NAME.tar
cd ..

NAME=ipp-solaris-$IPP_VERSION.x86
mkdir package/$NAME
FILE_LIST=`find . -type f -name "*" | grep -v ".svn" | grep -v "./nutconf-dummy" | grep -v "./make_package.sh" | grep -v "nut/" | grep -v install.log | grep -v uninstall.log | grep -v "package/" | grep -v aix | grep -v hpux | grep -v "solari/"`
cp --parents -r $FILE_LIST package/$NAME
cd package
tar cpvf $NAME.tar $NAME
gzip -9 $NAME.tar
cd ..

NAME=ipp-hpux-$IPP_VERSION.parisc
mkdir package/$NAME
FILE_LIST=`find . -type f -name "*" | grep -v ".svn" | grep -v "./nutconf-dummy" | grep -v "./make_package.sh" | grep -v "nut/" | grep -v install.log | grep -v uninstall.log | grep -v "package/" | grep -v aix | grep -v solint | grep -v "solari/"`
cp --parents -r $FILE_LIST package/$NAME
cd package
tar cpvf $NAME.tar $NAME
gzip -9 $NAME.tar
cd ..

NAME=ipp-aix-$IPP_VERSION.powerpc
mkdir package/$NAME
FILE_LIST=`find . -type f -name "*" | grep -v ".svn" | grep -v "./nutconf-dummy" | grep -v "./make_package.sh" | grep -v "nut/" | grep -v install.log | grep -v uninstall.log | grep -v "package/" | grep -v hpux | grep -v solint | grep -v "solari/"`
cp --parents -r $FILE_LIST package/$NAME
cd package
tar cpvf $NAME.tar $NAME
gzip -9 $NAME.tar
cd ..
