# Network UPS Tools: data/html

prefix		= /usr
exec_prefix	= ${prefix}

HTMLPATH	= $(DESTDIR)${prefix}/html

INSTALLCMD  = /usr/bin/install -c
INSTALLMODE = 0644

DATA_FILES  = index.html header.html bottom.html nut-banner.png

all:

clean:

install-cgi-html:
	@if (test ! -d $(HTMLPATH)) then \
		echo ../../install-sh -d $(HTMLPATH) || exit 1;\
		../../install-sh -d $(HTMLPATH) || exit 1;\
	fi
	@for f in $(DATA_FILES) ; do \
		echo $(INSTALLCMD) -m $(INSTALLMODE) $$f $(HTMLPATH); \
		$(INSTALLCMD) -m $(INSTALLMODE) $$f $(HTMLPATH); \
	done
