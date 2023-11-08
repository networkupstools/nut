# See also: .github/workflows/PyNUTClient.yml
# Note: this Makefile is focused on PyPI publication
# The usual autotools stuff including clean-up is in parent dir
# (to allow easier mixing of the module and app, if/when desired)

all: src

NUT_SOURCE_GITREV_NUMERIC = @NUT_SOURCE_GITREV_NUMERIC@
PYTHON = @PYTHON@

GENERATED_DIST = dist build *.egg-info

$(GENERATED_DIST): .pypi-dist

clean-local:
	rm -rf src $(GENERATED_DIST)
	rm -f .pypi-src .pypi-dist

MAINTAINERCLEANFILES = Makefile.in .dirstamp .pypi-tools*

src: .pypi-src

upload publish:
	case x"$(NUT_SOURCE_GITREV_NUMERIC)" in \
		v{0,1,2,3,4,5,6,7,8,9}*) $(MAKE) $(AM_FLAGS) upload-pypi ;; \
		*) $(MAKE) $(AM_FLAGS) upload-testpypi ;; \
	esac

.pypi-src: test_nutclient.py.in PyNUT.py.in setup.py.in Makefile
	rm -rf src "$@"
	mkdir src
	for S in "$(srcdir)"/*.py.in ; do \
		B="`basename "$${S}" .in`" ; \
		if test x"$${B}" = xsetup.py ; then \
			if ! test -s "$${B}" ; then \
				sed -e "s/[@]NUT_SOURCE_GITREV_NUMERIC[@]/$(NUT_SOURCE_GITREV_NUMERIC)/" < "$(srcdir)/$${B}.in" > "$${B}" || exit ; \
			fi ; \
			continue; \
		fi; \
		if test -s "$${B}" ; then \
			cp -pf "$${B}" src/ || exit ; \
			continue; \
		fi ; \
		sed -e "s,[@]PYTHON[@],@PYTHON@," < "$(srcdir)/$${B}.in" > "src/$${B}" || exit ; \
		if test -x "$(srcdir)/$${B}.in" ; then chmod +x "src/$${B}"; fi ; \
	done
	touch "$@"

.pypi-tools-dist:
	test -n "$(PYTHON)" && command -v $(PYTHON)
	$(PYTHON) -m pip install wheel
	touch "$@"

# Install via OS packaging or pip?
# https://twine.readthedocs.io/en/stable/
.pypi-tools-upload:
	test -n "$(PYTHON)" && command -v $(PYTHON)
	$(PYTHON) -m pip install twine
	command -v twine
	test -s $(HOME)/.pypirc
	touch "$@"

# Using pypa/setuptools
.pypi-dist: .pypi-tools-dist .pypi-src
	rm -rf $(GENERATED_DIST) "$@"
	$(PYTHON) setup.py sdist bdist_wheel
	touch "$@"

# These need $HOME/.pypirc prepared with credentials (API tokens) from
# https://pypi.org/manage/account/ and https://test.pypi.org/manage/account/
# TODO: .asc/.sig files for releases?
upload-pypi: .pypi-dist .pypi-tools-upload
	twine upload dist/*

upload-testpypi: .pypi-dist .pypi-tools-upload
	twine upload -r testpypi dist/*
