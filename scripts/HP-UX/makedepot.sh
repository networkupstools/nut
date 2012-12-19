#!/bin/bash

set -x 

CUR_DIR=$(pwd)
TOP_DIR=$CUR_DIR/../..
INSTALL_DIR=/usr/local
DEST_DIR=$CUR_DIR/nut_install

rm -rf $DEST_DIR

cd $TOP_DIR
./configure --prefix=$INSTALL_DIR --with-dev
make 1>/dev/null

#We need to install this or libtool won't be able to relink some binaries (upsc
#for instance) and will install the shell wrapper instaead of the binary.
#libtool needs libupsclient.la to relink those binaries so we install it first.
make install
DESTDIR=$DEST_DIR make install
make uninstall

cd $CUR_DIR
swpackage -s nut.psf -d $CUR_DIR/nut.depot; \
echo "Execution completed" 

