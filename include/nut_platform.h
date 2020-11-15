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
	#define NUT_PLATFORM_APPLE_MACH

	#include <TargetConditionals.h>

	#if (defined TARGET_OS_EMBEDDED)
		/** iOS (implies \ref NUT_PLATFORM_APPLE_MACH) */
		#define NUT_PLATFORM_APPLE_IOS
	#endif
	#if (defined TARGET_IPHONE_SIMULATOR)
		/** iOS simulator (implies \ref NUT_PLATFORM_APPLE_MACH) */
		#define NUT_PLATFORM_APPLE_IOS_SIMULATOR
	#endif
	#if (defined TARGET_OS_IPHONE)
		/** iPhone (implies \ref NUT_PLATFORM_APPLE_MACH) */
		#define NUT_PLATFORM_APPLE_IPHONE
	#endif
	#if (defined TARGET_OS_MAC)
		/** Mac OS X (implies \ref NUT_PLATFORM_APPLE_MACH) */
		#define NUT_PLATFORM_APPLE_OSX
	#endif
#endif

/*
 * GCC AIX issue: __unix__ nor __unix are not defined in older GCC
 * Addressed in GCC 4.7.0, see
 * http://gcc.gnu.org/bugzilla/show_bug.cgi?id=39950
 * Remove if no longer necessary
 */
#if (defined _AIX && !defined __unix__)
	#define __unix__
#endif

/* Microsoft Windows */
#if (defined _WIN32 || defined _WIN64)
	/** Windows */
	#define NUT_PLATFORM_MS_WINDOWS

	#if (defined NTDDI_WIN8 && NTDDI_VERSION >= NTDDI_WIN8)
		/** Windows 8 */
		#define NUT_PLATFORM_MS_WINDOWS8
	#endif

/* UNIX */
/* Note that Apple OSX doesn't define __unix__ nor __unix; are they ashamed or something? */
#elif (defined __unix__ || defined __unix || defined NUT_PLATFORM_APPLE_MACH)
	#include <sys/param.h>
	#include <unistd.h>

	/** UNIX */
	#define NUT_PLATFORM_UNIX

	#if (defined _POSIX_VERSION)
		/** POSIX (implies \ref NUT_PLATFORM_UNIX), expands to POSIX version */
		#define NUT_PLATFORM_POSIX _POSIX_VERSION
	#endif

	#if (defined __linux__)
		/** Linux (implies \ref NUT_PLATFORM_UNIX) */
		#define NUT_PLATFORM_LINUX
	#endif
	#if (defined __sun && defined __SVR4)
		/** Solaris (implies \ref NUT_PLATFORM_UNIX) */
		#define NUT_PLATFORM_SOLARIS
	#endif
	#if (defined __hpux)
		/** Hewlett-Packard HP-UX (implies \ref NUT_PLATFORM_UNIX) */
		#define NUT_PLATFORM_HPUX
	#endif
	#if (defined _AIX)
		/** AIX (implies \ref NUT_PLATFORM_UNIX) */
		#define NUT_PLATFORM_AIX
	#endif

	/* Note that BSD is defined in sys/param.h */
	#if (defined BSD)
		/** BSD (implies \ref NUT_PLATFORM_UNIX) */
		#define NUT_PLATFORM_BSD

		#if (defined __DragonFly__)
			/** DragonFly (implies \ref NUT_PLATFORM_UNIX, \ref NUT_PLATFORM_BSD) */
			#define NUT_PLATFORM_DRAGONFLY
		#elif (defined __FreeBSD__)
			/** FreeBSD (implies \ref NUT_PLATFORM_UNIX, \ref NUT_PLATFORM_BSD) */
			#define NUT_PLATFORM_FREEBSD
		#elif (defined __OpenBSD__)
			/** OpenBSD (implies \ref NUT_PLATFORM_UNIX, \ref NUT_PLATFORM_BSD) */
			#define NUT_PLATFORM_OPENBSD
		#elif (defined __NetBSD__)
			/** NetBSD (implies \ref NUT_PLATFORM_UNIX, \ref NUT_PLATFORM_BSD) */
			#define NUT_PLATFORM_NETBSD
		#endif
	#endif
#endif

#endif  /* NUT_PLATFORM_H_SEEN */

