
EXTRA_DIST = gen-nutupsconf-aug.py nutupsconf.aug.tpl \
	README tests/test_nut.aug

# only call the script to generate Augeas ups.conf lens upon "make dist",
# and if Python is present
dist-hook:
	@if test -x /usr/bin/python; then \
		echo "Regenerating Augeas ups.conf lens."; \
		$(distdir)/gen-nutupsconf-aug.py $(distdir)/; \
	else \
		echo "----------------------------------------------------------------------"; \
		echo "Warning: Python is not available."; \
		echo "Skipping Augeas ups.conf lens regeneration."; \
		echo "----------------------------------------------------------------------"; \
	fi
