# directory definitions
INSTALLSH = ../../install-sh
CONFIGPATH    = /etc/rc.config.d
SCRIPTPATH    = /sbin/init.d
LINKPATH	= /sbin/rc3.d
LINKPREFIX	= 991
OWNER	=	root
GROUP	= 	root
SCRIPTS = nut-upsd.sh nut-drvctl.sh nut-upsmon.sh
CONFIGS = nut-drvctl nut-upsd nut-upsmon

INSTALLPERMS = 0755
INSTALLCMD = $(INSTALLSH) -c

all: install


install: 
	@if (test ! -d $(CONFIGPATH)) then \
		echo "NO $(CONFIGPATH)"; exit 1; \
	fi
	@if (test ! -d $(SCRIPTPATH)) then \
		echo "NO $(SCRIPTPATH)"; exit 1; \
	fi
	@if (test ! -d $(LINKPATH)) then \
		echo "NO $(LINKPATH)"; exit 1; \
	fi
	@for script in $(SCRIPTS); do\
		name=`basename $$script .sh` ; \
		$(INSTALLSH) -c -o $(OWNER) -g $(GROUP) $$script $(SCRIPTPATH)/$$name || exit 1; \
		ln -s $(SCRIPTPATH)/$$name $(LINKPATH)/K$(LINKPREFIX)$$name \
		|| echo "Unable to link $(SCRIPTPATH)/$$name to $(LINKPATH)/K$(LINKPREFIX)$$name"; \
		ln -s $(SCRIPTPATH)/$$name $(LINKPATH)/S$(LINKPREFIX)$$name  \
		|| echo "Unable to link $(SCRIPTPATH)/$$name to $(LINKPATH)/S$(LINKPREFIX)$$name"; \
	done
	@for config in $(CONFIGS); do\
		$(INSTALLSH) -c -o $(OWNER) -g $(GROUP) $$config $(CONFIGPATH) || exit 1 ; \
	done

remove:
	@for config in $(CONFIGS); do\
		rm -f $(CONFIGPATH)/$$config;\
	done
	@for script in $(SCRIPTS); do\
		name=`basename $$script .sh` ; \
		rm -f $(SCRIPTPATH)/$$name || exit 1; \
		rm -f $(LINKPATH)/K$(LINKPREFIX)$$name \
		|| echo "Unable to remove $(LINKPATH)/K$(LINKPREFIX)$$name"; \
		rm -f $(LINKPATH)/S$(LINKPREFIX)$$name  \
		|| echo "Unable to remove $(LINKPATH)/S$(LINKPREFIX)$$name"; \
	done
