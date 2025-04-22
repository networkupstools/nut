/* cpputest - basic runner for unit tests

   Copyright (C)
	2012	Emilien Kia <emilienkia-guest@alioth.debian.org>
	2020-2025	Jim Klimov <jimklimov+nut@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "common.h"

#include <stdexcept>
#include <cstdlib>

/* Current CPPUnit offends the honor of C++98 and maybe later versions */
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SUGGEST_OVERRIDE_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SUGGEST_DESTRUCTOR_OVERRIDE_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_WEAK_VTABLES_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DYNAMIC_EXCEPTION_SPEC_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXTRA_SEMI_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_OLD_STYLE_CAST_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_ZERO_AS_NULL_POINTER_CONSTANT_BESIDEFUNC)
#pragma GCC diagnostic push
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS
#  pragma GCC diagnostic ignored "-Wglobal-constructors"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS
#  pragma GCC diagnostic ignored "-Wexit-time-destructors"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SUGGEST_OVERRIDE_BESIDEFUNC
#  pragma GCC diagnostic ignored "-Wsuggest-override"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SUGGEST_DESTRUCTOR_OVERRIDE_BESIDEFUNC
#  pragma GCC diagnostic ignored "-Wsuggest-destructor-override"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_WEAK_VTABLES_BESIDEFUNC
#  pragma GCC diagnostic ignored "-Wweak-vtables"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DYNAMIC_EXCEPTION_SPEC_BESIDEFUNC
#  pragma GCC diagnostic ignored "-Wdeprecated-dynamic-exception-spec"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXTRA_SEMI_BESIDEFUNC
#  pragma GCC diagnostic ignored "-Wextra-semi"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_OLD_STYLE_CAST_BESIDEFUNC
#  pragma GCC diagnostic ignored "-Wold-style-cast"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_ZERO_AS_NULL_POINTER_CONSTANT_BESIDEFUNC
/* Note: this silencer is only needed here due to test bodies */
#  pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
# endif
#endif
#if (defined __clang__) && (defined HAVE_PRAGMA_CLANG_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS)
# ifdef HAVE_PRAGMA_CLANG_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS
#  pragma clang diagnostic push "-Wdeprecated-declarations"
# endif
#endif

#include <cppunit/TestResult.h>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/TextTestProgressListener.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

extern "C" {
#include "timehead.h"

/* Let tests also see this flag */
extern bool verbose;
}

bool verbose = false;

// Inspired by https://stackoverflow.com/a/66702001
class MyCustomProgressTestListener : public CppUnit::TextTestProgressListener {
    public:
        virtual void startTest(CppUnit::Test *test) override;
};

// Implement out of class declaration to avoid
//   error: 'MyCustomProgressTestListener' has no out-of-line virtual method
//   definitions; its vtable will be emitted in every translation unit
//   [-Werror,-Wweak-vtables]
void MyCustomProgressTestListener::startTest(CppUnit::Test *test) {
    //fprintf(stderr, "starting test %s\n", test->getName().c_str());
    std::cerr << "starting test " << (test == nullptr ? "<null>" : test->getName()) << std::endl << std::flush;
}

int main(int argc, char* argv[])
{
  if (argc > 1) {
    if (strcmp("-v", argv[1]) == 0 || strcmp("--verbose", argv[1]) == 0 ) {
      verbose = true;
    }
  }

  ::srand(static_cast<unsigned int>(::time(nullptr)));

  /* Get the top level suite from the registry */
  std::cerr << "D: Getting test suite..." << std::endl;
  CppUnit::Test *suite = CppUnit::TestFactoryRegistry::getRegistry().makeTest();

  /* Adds the test to the list of test to run */
  std::cerr << "D: Preparing test runner..." << std::endl;
  CppUnit::TextUi::TestRunner runner;
  runner.addTest( suite );

  /* Change the default outputter to a compiler error format outputter */
  std::cerr << "D: Setting test runner outputter..." << std::endl;
  runner.setOutputter( new CppUnit::CompilerOutputter( &runner.result(),
                                                       std::cerr ) );

  if (verbose) {
    /* Add a listener to report test names */
    std::cerr << "D: Setting test runner listener for test names..." << std::endl;
    /* Only allocate when needed; static to avoid freeing */
    static MyCustomProgressTestListener progress;
    runner.eventManager().addListener(&progress);
  }

  /* Run the tests. */
  bool wasSucessful = false;
  try {
    std::cerr << "D: Launching the test run..." << std::endl;
    wasSucessful = runner.run();
  }
  catch ( std::invalid_argument &e )  // Test path not resolved
  {
    std::cerr  << std::endl
               << "ERROR: " <<  e.what()
               << std::endl;
    wasSucessful = false;
  }

  /* Return error code 1 if the one of test failed. */
  std::cerr << "D: Got to the end of test suite with code " <<
    "'" << ( wasSucessful ? "true" : "false" ) << "'" << std::endl;
  return wasSucessful ? 0 : 1;
}

#if (defined __clang__) && (defined HAVE_PRAGMA_CLANG_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS)
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SUGGEST_OVERRIDE_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SUGGEST_DESTRUCTOR_OVERRIDE_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_WEAK_VTABLES_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DYNAMIC_EXCEPTION_SPEC_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXTRA_SEMI_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_OLD_STYLE_CAST_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_ZERO_AS_NULL_POINTER_CONSTANT_BESIDEFUNC)
# pragma GCC diagnostic pop
#endif
