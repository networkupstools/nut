#!/bin/sh
# test script to generate the website
# its sole purpose ATM is to track the command line used
# to generate the output

VERS="2.6.0"
DATE="2008-06-25"

# Leave the desired layout uncommented.
#LAYOUT=layout1		# Tables based layout.
LAYOUT=layout2		# CSS based simulated frames layout.

ASCIIDOC_HTML="asciidoc --unsafe --backend=xhtml11 --conf-file=${LAYOUT}.conf --attribute icons --attribute iconsdir=./images/icons --attribute=badges --attribute=revision=$VERS  --attribute=date=$DATE"

$ASCIIDOC_HTML -a index-only website.txt
#$ASCIIDOC_HTML -a toc -a numbered userguide.txt
#$ASCIIDOC_HTML -d manpage manpage.txt
#$ASCIIDOC_HTML downloads.txt
#$ASCIIDOC_HTML latex-backend.txt
#$ASCIIDOC_HTML README.txt
#$ASCIIDOC_HTML INSTALL.txt
#$ASCIIDOC_HTML CHANGELOG.txt
#$ASCIIDOC_HTML README-website.txt
#$ASCIIDOC_HTML support.txt
#$ASCIIDOC_HTML source-highlight-filter.txt
#$ASCIIDOC_HTML music-filter.txt
#$ASCIIDOC_HTML a2x.1.txt
#$ASCIIDOC_HTML asciimath.txt
