EXTRA_DIST = nit.sh README

check: check-NIT

# Run in builddir, use script from srcdir
# Avoid running "$<" - not all make implementations handle that
check-NIT: $(abs_srcdir)/nit.sh
	"$(abs_srcdir)/nit.sh"

# Make sure pre-requisites for NIT are fresh as we iterate
check-NIT-devel: $(abs_srcdir)/nit.sh
	@cd .. && ( $(MAKE) -s cppnit || echo "OPTIONAL C++ test client test will be skipped" )
	@cd "$(top_builddir)/clients" && $(MAKE) -s upsc upsrw upsmon
	@cd "$(top_builddir)/server" && $(MAKE) -s upsd
	@cd "$(top_builddir)/drivers" && $(MAKE) -s dummy-ups
	@$(MAKE) check-NIT

MAINTAINERCLEANFILES = Makefile.in .dirstamp

clean-local:
	rm -rf tmp
