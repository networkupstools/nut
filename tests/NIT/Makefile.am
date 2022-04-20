EXTRA_DIST = nit.sh README

check: check-NIT

# Run in builddir, use script from srcdir
# Avoid running "$<" - not all make implementations handle that
check-NIT: $(abs_srcdir)/nit.sh
	@cd .. && ( $(MAKE) -s cppnit || echo "OPTIONAL C++ test client test will be skipped" )
	"$(abs_srcdir)/nit.sh"

MAINTAINERCLEANFILES = Makefile.in .dirstamp

clean-local:
	rm -rf tmp
