# See also: .github/workflows/PyNUTClient.yml
# Note: this Makefile is focused on PyPI publication
# The usual autotools stuff including clean-up is in parent dir
# (to allow easier mixing of the module and app, if/when desired)

# Non-maintainers can at most generate the source layout for python setuptools
# (having only shell scripting as a prerequisite suffices for that)
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

# Tagged releases should only have three blocks of digits separated by dots
upload publish:
	@echo "  PYPI  Checking upload type for module version '$(NUT_SOURCE_GITREV_NUMERIC)'" ; \
	 case x"`echo "$(NUT_SOURCE_GITREV_NUMERIC)" | tr -d '[0-9]'`" in \
		x..) echo "  PYPI  ...release"; $(MAKE) $(AM_FLAGS) upload-pypi ;; \
		x*)  echo "  PYPI  ...testing"; $(MAKE) $(AM_FLAGS) upload-testpypi ;; \
	 esac

# README.txt is also a part of module standard expectations
.pypi-src: test_nutclient.py.in PyNUT.py.in setup.py.in README.adoc Makefile
	@echo "  PYPI  Generate PyPI module source"
	@rm -rf src "$@"
	@mkdir src
	@for S in "$(srcdir)"/*.py.in ; do \
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
	 done ; \
	 cp -pf "$(srcdir)/README.adoc" src/README.txt || exit
	@touch "$@"

.pypi-tools-dist:
	@echo "  PYPI  Prepare PyPI tools to generate a distribution"
	@test -n "$(PYTHON)" && command -v $(PYTHON)
	@$(PYTHON) -m pip install wheel
	@touch "$@"

# Install via OS packaging or pip?
# https://twine.readthedocs.io/en/stable/
.pypi-tools-upload:
	@echo "  PYPI  Prepare PyPI tools and resources to upload a distribution"
	@test -n "$(PYTHON)" && command -v $(PYTHON)
	@$(PYTHON) -m pip install twine
	@command -v twine
	@test -s $(HOME)/.pypirc
	@touch "$@"

# Using pypa/setuptools
.pypi-dist: .pypi-tools-dist .pypi-src
	@echo "  PYPI  Generate PyPI module distribution"
	@rm -rf $(GENERATED_DIST) "$@"
	@$(PYTHON) setup.py sdist bdist_wheel
	@touch "$@"

# These need $HOME/.pypirc prepared with credentials (API tokens) from
# https://pypi.org/manage/account/ and https://test.pypi.org/manage/account/
# TODO: .asc/.sig files for releases?
upload-pypi: .pypi-dist .pypi-tools-upload
	@echo "  PYPI  Upload PyPI module distribution to production/release PyPI repository"
	@twine upload dist/*

upload-testpypi: .pypi-dist .pypi-tools-upload
	@echo "  PYPI  Upload PyPI module distribution to testing/staging PyPI repository"
	@twine upload -r testpypi dist/*
