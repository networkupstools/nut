# Network UPS Tools: data/html

EXTRA_DIST_PY2GTK2 = \
			 app/ui/gui-1.3.glade \
			 app/NUT-Monitor-py2gtk2.in \
			 app/nut-monitor-py2gtk2.desktop

EXTRA_DIST_PY3QT5 = \
			 app/ui/aboutdialog1.ui \
			 app/ui/dialog1.ui \
			 app/ui/dialog2.ui \
			 app/ui/window1.ui \
			 app/NUT-Monitor-py3qt5.in \
			 app/nut-monitor-py3qt5.desktop

EXTRA_DIST = README \
			 app/nut-monitor.appdata.xml \
			 app/icons/48x48/nut-monitor.png \
			 app/icons/64x64/nut-monitor.png \
			 app/icons/256x256/nut-monitor.png \
			 app/icons/scalable/nut-monitor.svg \
			 app/README \
			 app/pixmaps/var-rw.png \
			 app/pixmaps/on_line.png \
			 app/pixmaps/warning.png \
			 app/pixmaps/on_battery.png \
			 app/pixmaps/var-ro.png \
			 app/locale/fr/LC_MESSAGES/NUT-Monitor.mo \
			 app/locale/it/LC_MESSAGES/NUT-Monitor.mo \
			 module/PyNUT.py.in \
			 module/test_nutclient.py.in

# TODO: Make py2/py3-only builds, delivered preferred symlinks, etc. optional:
EXTRA_DIST += $(EXTRA_DIST_PY2GTK2)
EXTRA_DIST += $(EXTRA_DIST_PY3QT5)

MAINTAINERCLEANFILES = Makefile.in .dirstamp
