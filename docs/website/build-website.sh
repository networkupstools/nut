#!/bin/sh
# test script to generate the website
# its sole purpose ATM is to track the command line used
# to generate the output

VERS="2.6.0"
DATE="2008-06-25"

# CSS based simulated frames layout.
LAYOUT=web-layout

ASCIIDOC_HTML="asciidoc --unsafe --backend=xhtml11 --conf-file=${LAYOUT}.conf --attribute icons --attribute iconsdir=./icons --attribute=badges --attribute=revision=$VERS  --attribute=date=$DATE"

# Cleanup
rm -rf output

# base tree
mkdir output
cd output && ln -s ../../images .  && ln -s ../../icons . && cp ../faviconut.* . \
&& cp ../*css . && cd ..

$ASCIIDOC_HTML -o output/index.html -a index-only website.txt
$ASCIIDOC_HTML -o output/download.html download.txt
$ASCIIDOC_HTML -o output/projects.html projects.txt
$ASCIIDOC_HTML -o output/stable-hcl.html stable-hcl.txt
$ASCIIDOC_HTML -o output/documentation.html documentation.txt
$ASCIIDOC_HTML -o output/support.html support.txt
$ASCIIDOC_HTML -o output/acknowledgements.html acknowledgements.txt
$ASCIIDOC_HTML -o output/features.html ../features.txt
$ASCIIDOC_HTML -o output/FAQ.html -a toc ../FAQ
