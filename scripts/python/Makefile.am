# Network UPS Tools: data/html

NUT_MONITOR_PY2GTK2 = \
	app/ui/gui-1.3.glade \
	app/nut-monitor-py2gtk2.desktop

NUT_MONITOR_PY2GTK2_TEMPLATE = \
	app/NUT-Monitor-py2gtk2.in

NUT_MONITOR_PY2GTK2_GENERATED = \
	app/NUT-Monitor-py2gtk2

NUT_MONITOR_PY3QT5 = \
	app/ui/aboutdialog1.ui \
	app/ui/dialog1.ui \
	app/ui/dialog2.ui \
	app/ui/window1.ui \
	app/nut-monitor-py3qt5.desktop

NUT_MONITOR_PY3QT5_TEMPLATE = \
	app/NUT-Monitor-py3qt5.in

NUT_MONITOR_PY3QT5_GENERATED = \
	app/NUT-Monitor-py3qt5

NUT_MONITOR_COMMON = \
	README \
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
	app/locale/it/LC_MESSAGES/NUT-Monitor.mo

NUT_MONITOR_COMMON_TEMPLATE = \
	module/PyNUT.py.in \
	module/test_nutclient.py.in

NUT_MONITOR_COMMON_GENERATED = \
	module/PyNUT.py \
	module/test_nutclient.py

# For now, we have a "dispatcher" script and applet manifest,
# to select a functional choice of the GUI client, if possible:
NUT_MONITOR_DISPATCHER = \
	app/NUT-Monitor \
	app/nut-monitor.desktop

# `make dist` tarball contents:
EXTRA_DIST = \
	$(NUT_MONITOR_DISPATCHER) \
	$(NUT_MONITOR_COMMON) $(NUT_MONITOR_COMMON_TEMPLATE)

# TODO: Make py2/py3-only builds, delivered preferred symlinks, etc. optional?
EXTRA_DIST += $(NUT_MONITOR_PY2GTK2) $(NUT_MONITOR_PY2GTK2_TEMPLATE)
EXTRA_DIST += $(NUT_MONITOR_PY3QT5) $(NUT_MONITOR_PY3QT5_TEMPLATE)

MAINTAINERCLEANFILES = Makefile.in .dirstamp

clean-local:
	rm -rf *.pyc __pycache__ */*.pyc */__pycache__ */*/*.pyc */*/__pycache__
