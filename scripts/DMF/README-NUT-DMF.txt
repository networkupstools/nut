= DMF Docs referral

See `$NUTSRC/docs/nut-dmf.txt` for details about DMF format, helper tools and
implementation.

Developers with GNU make might succeed doing quick hacks without automake'ing
the whole project, by running `gmake -f GNUMakefile-local.mk dmf` - but YMMV...

== Troubleshooting the builds

=== `dmfify-mib.sh` and `nut_cpp`

These can fail during the `configure` script run (detecting that we can not
re-generate DMF files on the current platform) or later in the actual build
if you somehow work around the former check (e.g. during dev-testing).

==== `nut_cpp` pre-processed code filtering

Most of the hassle is about adapting `nut_cpp` script filters to system headers
and compiler preferences (including their private headers) as target platforms
are changed.  This is generally manifested as a Python stack trace.

The `pycparser` module involved in conversion of C structures to DMF XML is
very discerning in its tastes, and errors out on many keywords that are not
part of standard C (or were not at the time your installed module version
was coded), such as `__attribute__((...))` expressions.

You can reproduce the DMF file generation from command-line, to troubleshoot
the procedure quickly (compared to iterating by full NUT rebuilds) and to make
sure your fix eventually works for the current system.

Generally, this is prototyped by what `configure.ac` does with a temporary
location, but you can craft one in the source tree:

----
:; cd scripts/DMF
:; ln -fs ../../drivers/apc-mib.c

# If configure script did not succeed, header was not generated
:; if [ ! -s ../../include/config.h ] ; then \
    rm -f config.h ; touch config.h ; \
   fi

# Run the parser:
:; CFLAGS="-I../../drivers -I../../include -I." \
   DEBUG_NUT_CPP=true DEBUG=1 \
   ./dmfify-mib.sh apc-mib.c
----

Other environment variables can be customized, such as `CC` and `CPP`, if
needed (see the scripts for up-to-date inspiration).  Additional `CFLAGS`
settings may be desirable (e.g. `-Werror -Wall`) and debug print-outs from
the failed build (or `config.log`) can suggest the values you would want to
copy-paste in order to faithfully reproduce the failed use-case.

The `DEBUG_NUT_CPP=true` part is crucial for saving the `temp-cpp-filt.tmp`
which is the product of pre-processing and subsequent brute-force filtering
in `nut_cpp`, and becomes the C code that `pycparser` tries to chew through.

You should inspect this file, searching for the header names mentioned by
the Python stack trace, and there would usually be comments with file names
and line numbers left by preprocessor to help navigate in the resulting file.
Look for anything fishy, from remaining keywords with `__` in their names
(usually because there are complex arguments in parentheses that `nut_cpp`
did not match yet), to lines that do not make sense because we chopped off
too much, like `typedef float float;`

==== Other script problems

Another approach, when the problem is about updating the shell or python
scripts and not the `nut_cpp` filter, can be to use `make` while iterating
in order to reproduce the realistic build conditions, e.g. with this build
configuration tailored to ensure DMF requirements and not waste time on
other aspects like docs:

----
:; ./autogen.sh && \
   ./configure --with-all=auto --with-dmf --with-dmf_lua \
    --with-docs=no --with-ssl=no \
    --enable-Werror --enable-warnings --disable-Wcolor --enable-silent-rules \
    --with-dmfsnmp-regenerate --with-dmfnutscan-regenerate \
    --with-dmfsnmp-validate --with-dmfnutscan-validate \
    # CC=clang CXX=clang++ CFLAGS='-std=gnu99 ' CXXFLAGS='-std=gnu++11 '
----

Then as you iterate the script code, just re-run `make` to build and test
a single file until you like the results:

----
:; ( cd scripts/DMF && rm -f apc* dmfsnmp/apc-mib.dmf && \
     make V=1 DEBUG_NUT_CPP=true jsonify-mib.py dmfsnmp/apc-mib.dmf )
----

NOTE: The pre-processed file saved with `DEBUG_NUT_CPP=true` may appear
as `dmfsnmp/temp-cpp-filt.tmp`.

Also take a look at lines debug-logged as `PREPROCESS: (...)` with the
actually used call to the preprocessor (before scripted filtering):
you can copy-paste that command and store a copy of preprocessed-only
file to investigate the differences imposed by our script.

==== Hacking into `pycparser`

In complex cases you can edit your copy of `pycparser::ply/yacc.py`
method `parse`, to default its parameter `debug=True`. Note that this
generates many megabytes of detailed logs, and a parsing session can
take tens of minutes.

A better alternative is to inject logging of tokens into `parseopt_notrack()`
(yes there are warning comments that it is generated and the change would be
lost... upon rebuild of `pycparser` itself). Add the following line just
after the block which assigns `lookahead = ...` from one or another data
source (for some reasons, this prints a few duplicate lines per token, so
we use `uniq` below to get a better readable output):
----
            sys.stderr.write("[D] %s\n" % str(lookahead))
----

Either way, don't forget to change it back afterwards.

----
:; ( cd scripts/DMF && rm -f apc* dmfsnmp/apc-mib.dmf && \
     make V=1 DEBUG_NUT_CPP=true jsonify-mib.py dmfsnmp/apc-mib.dmf ) \
     | uniq > parse-debug.log 2>&1
----

NOTE: Unfortunately there does not seem to be a way to just pass this
parameter with the code path used in `jsonify-mib.py` calling `parse_file`:
their `__init__.py` method `parse_file()` takes (or spawns) a `parser` and
launches it without a `debuglevel` argument (seen in `c_parser.py` method
`parse()`). Experiments to make a copy of that method in our script and
extend it were not as fruitful as the hacked-in line documented above...

Hope this helps,
Jim Klimov
