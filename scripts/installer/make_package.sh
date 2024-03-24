#!/usr/bin/env bash

#	make_package.sh
#	Copyright (c) 2013-2015, by Eaton (R) Corporation. All rights reserved.
#	Unlike what the name implies, this script prepares tarballs for each
#	supported platform, including a separately prepared platform package
#	with compiled NUT and adds third-party libs and Eaton IPP - Unix
#	scripts for better taste ;)

. ./version.sh || exit $?

rm -Rf package
mkdir package

# NOTE: Originally this pulled installer sources (separate from NUT code base)
# If this script were to be modernized, it could be prudent to `make package`
# in NUT sources for each platform, to create the package file(s) tarballed
# below for end-user along with the interactive installer delivery.
#git pull --all
# [ $? = 0 ] && git merge upstream/master || exit $?

# NOTE: See README.adoc about expected subdirectory contents with binary files
NAME="ipp-solaris-$IPP_VERSION.sparc"
mkdir "package/$NAME"
FILE_LIST="`find . -type f -name '*' | egrep -v '.svn|.git|./nutconf-dummy|./make_package.sh|nut/|(un|)install.log|package/|aix|hpux|solint'`"
cp --parents -r $FILE_LIST "package/$NAME"
cd package
tar cpvf "$NAME.tar" "$NAME"
gzip -9 "$NAME.tar"
cd ..

NAME="ipp-solaris-$IPP_VERSION.x86"
mkdir "package/$NAME"
FILE_LIST="`find . -type f -name '*' | egrep -v '.svn|.git|./nutconf-dummy|./make_package.sh|nut/|(un|)install.log|package/|aix|hpux|solari/'`"
cp --parents -r $FILE_LIST "package/$NAME"
cd package
tar cpvf "$NAME.tar" "$NAME"
gzip -9 "$NAME.tar"
cd ..

NAME="ipp-hpux-$IPP_VERSION.parisc"
mkdir "package/$NAME"
FILE_LIST="`find . -type f -name '*' | egrep -v '.svn|.git|./nutconf-dummy|./make_package.sh|nut/|(un|)install.log|package/|aix|solint|solari/|solcmn/'`"
cp --parents -r $FILE_LIST "package/$NAME"
cd package
tar cpvf "$NAME.tar" "$NAME"
gzip -9 "$NAME.tar"
cd ..

NAME="ipp-aix-$IPP_VERSION.powerpc"
mkdir "package/$NAME"
FILE_LIST="`find . -type f -name '*' | egrep -v '.svn|.git|./nutconf-dummy|./make_package.sh|nut/|(un|)install.log|package/|hpux|solint|solari/|solcmn/'`"
cp --parents -r $FILE_LIST "package/$NAME"
cd package
tar cpvf "$NAME.tar" "$NAME"
gzip -9 "$NAME.tar"
cd ..
