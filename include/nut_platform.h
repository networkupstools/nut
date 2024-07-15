/**
 *  \brief  Platform-specific checks
 *
 *  The header performs checks to resolve the actual build platform.
 *  It defines macra that may be later used to produce platform-tailored
 *  code.
 *
 *  Be careful when writing platform-specific code; avoid that if possible.
 *
 *  References:
 *  http://nadeausoftware.com/articles/2012/01/c_c_tip_how_use_compiler_predefined_macros_detect_operating_system
 *
 *  \author  Vaclav Krpec  <VaclavKrpec@Eaton.com>
 *  \date    2012/10/12
 */

#ifndef NUT_PLATFORM_H_SEEN
#define NUT_PLATFORM_H_SEEN 1

/*
 * In case doxygen source doc isn't generated
 * (which is the case at time of writing this),
 * just check the doxygen-documented (i.e. "**"
 * prefixed) NUT_PLATFORM_* macra, below.
 */

/* Apple Mac OS X, iOS and Darwin */
#if (defined __APPLE__ && defined __MACH__)
	/** Apple OS based on Mach ukernel */
#	define NUT_PLATFORM_APPLE_MACH

	/* https://stackoverflow.com/a/2339910/4715872 */
#	ifndef SOEXT
#		define SOEXT ".dylib"
#	endif

#	include <TargetConditionals.h>

#	if (defined TARGET_OS_EMBEDDED)
		/** iOS (implies \ref NUT_PLATFORM_APPLE_MACH) */
#		define NUT_PLATFORM_APPLE_IOS
#	endif
#	if (defined TARGET_IPHONE_SIMULATOR)
		/** iOS simulator (implies \ref NUT_PLATFORM_APPLE_MACH) */
#		define NUT_PLATFORM_APPLE_IOS_SIMULATOR
#	endif
#	if (defined TARGET_OS_IPHONE)
		/** iPhone (implies \ref NUT_PLATFORM_APPLE_MACH) */
#		define NUT_PLATFORM_APPLE_IPHONE
#	endif
#	if (defined TARGET_OS_MAC)
		/** Mac OS X (implies \ref NUT_PLATFORM_APPLE_MACH) */
#		define NUT_PLATFORM_APPLE_OSX
#	endif
#endif

/*
 * GCC AIX issue: __unix__ nor __unix are not defined in older GCC
 * Addressed in GCC 4.7.0, see
 * http://gcc.gnu.org/bugzilla/show_bug.cgi?id=39950
 * Remove if no longer necessary
 */
#if (defined _AIX && !defined __unix__)
#	define __unix__
#endif

/* Microsoft Windows */
#if (defined _WIN32 || defined _WIN64)
	/** Windows */
#	define NUT_PLATFORM_MS_WINDOWS

#	ifndef SOEXT
#		define SOEXT ".dll"
#	endif

#	if (defined NTDDI_WIN8 && NTDDI_VERSION >= NTDDI_WIN8)
		/** Windows 8 */
#		define NUT_PLATFORM_MS_WINDOWS8
#	endif

/* UNIX */
/* Note that Apple OSX doesn't define __unix__ nor __unix; are they ashamed or something? */
#elif (defined __unix__ || defined __unix || defined NUT_PLATFORM_APPLE_MACH)
#	include <sys/param.h>
#	include <unistd.h>

	/** UNIX */
#	define NUT_PLATFORM_UNIX

#	if (defined _POSIX_VERSION)
		/** POSIX (implies \ref NUT_PLATFORM_UNIX), expands to POSIX version */
#		define NUT_PLATFORM_POSIX _POSIX_VERSION
#	endif

#	if (defined __linux__)
		/** Linux (implies \ref NUT_PLATFORM_UNIX) */
#		define NUT_PLATFORM_LINUX
#	endif
#	if (defined __sun && defined __SVR4)
		/** Solaris (implies \ref NUT_PLATFORM_UNIX) */
#		define NUT_PLATFORM_SOLARIS
#	endif
#	if (defined __hpux)
		/** Hewlett-Packard HP-UX (implies \ref NUT_PLATFORM_UNIX) */
#		define NUT_PLATFORM_HPUX

		/* Note: depending on CPU arch and OS version, library file
		 * name patterns here could have been "*.so" as well.
		 * E.g. per
		 * https://community.hpe.com/t5/operating-system-hp-ux/so-and-sl-files/td-p/3780528
		 *   *.sl are used in PA-RISC (11.11)
		 *   *.so shared libraries are used in HP-UX 11.20 and upwards.
		 * Integrity (Itanium-based) HPUX can use *.sl as well,
		 * but it is not recommended, see ld(1) under -lx:
		 *   https://web.archive.org/web/20090925153446/http://docs.hp.com/en/B2355-60103/ld.1.html
		 */
		/* FIXME: May want to detect better the CPU or OS version
		 *  to decide the SOEXT here*/
#		ifndef SOEXT
#			define SOEXT ".sl"
#		endif
#	endif
#	if (defined _AIX)
		/** AIX (implies \ref NUT_PLATFORM_UNIX) */
#		define NUT_PLATFORM_AIX
#	endif

	/* Note that BSD is defined in sys/param.h */
#	if (defined BSD)
		/** BSD (implies \ref NUT_PLATFORM_UNIX) */
#		define NUT_PLATFORM_BSD

#		if (defined __DragonFly__)
			/** DragonFly (implies \ref NUT_PLATFORM_UNIX, \ref NUT_PLATFORM_BSD) */
#			define NUT_PLATFORM_DRAGONFLY
#		elif (defined __FreeBSD__)
			/** FreeBSD (implies \ref NUT_PLATFORM_UNIX, \ref NUT_PLATFORM_BSD) */
#			define NUT_PLATFORM_FREEBSD
#		elif (defined __OpenBSD__)
			/** OpenBSD (implies \ref NUT_PLATFORM_UNIX, \ref NUT_PLATFORM_BSD) */
#			define NUT_PLATFORM_OPENBSD
#		elif (defined __NetBSD__)
			/** NetBSD (implies \ref NUT_PLATFORM_UNIX, \ref NUT_PLATFORM_BSD) */
#			define NUT_PLATFORM_NETBSD
#		endif
#	endif
#endif

/* not WIN32, not MACOS, not HPUX... */
#ifndef SOEXT
#	define SOEXT ".so"
#endif

#endif  /* NUT_PLATFORM_H_SEEN */

