EXTRA_DIST = nit.sh README

check: check-NIT

# Run in builddir, use script from srcdir
check-NIT: $(abs_srcdir)/nit.sh
	@cd .. && ( $(MAKE) -s cppnit || echo "OPTIONAL C++ test client test will be skipped" )
	"$<"

MAINTAINERCLEANFILES = Makefile.in .dirstamp

clean-local:
	rm -rf tmp
