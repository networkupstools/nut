#!/bin/sh
# test script to generate the documentation
# its sole purpose ATM is to track the command line used
# to generate the output

#a2x nut-asciidoc.txt
#asciidoc -a toc -a numbered nut-asciidoc.txt
#asciidoc -a index-only nut-asciidoc.txt

ASCIIDOC_HTML="asciidoc --unsafe --backend=xhtml11 --attribute icons --attribute iconsdir=./icons --attribute=badges -a toc -a numbered"

$ASCIIDOC_HTML -o html/packaging.html packaging.txt

#$ASCIIDOC_HTML -a toc -a numbered userguide.txt
