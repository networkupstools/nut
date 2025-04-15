#!@PYTHON@
# -*- coding: utf-8 -*-

# This source code is provided for testing/debuging purpose ;)

import PyNUT
import sys
import os

if __name__ == "__main__" :
    NUT_HOST = os.getenv('NUT_HOST', '127.0.0.1')
    NUT_PORT = int(os.getenv('NUT_PORT', '3493'))
    NUT_USER = os.getenv('NUT_USER', None)
    NUT_PASS = os.getenv('NUT_PASS', None)

    NUT_DEBUG = ("true" == os.getenv('DEBUG', 'false') or os.getenv('NUT_DEBUG_LEVEL', None) is not None)

    # Account "unexpected" failures (more due to coding than circumstances)
    # e.g. lack of protected access when no credentials were passed is okay
    failed = []

    print( "PyNUTClient test..." )
    #nut    = PyNUT.PyNUTClient( debug=True, port=NUT_PORT )
    #nut    = PyNUT.PyNUTClient( login=NUT_USER, password=NUT_PASS, debug=True, host=NUT_HOST, port=NUT_PORT )
    nut    = PyNUT.PyNUTClient( login=NUT_USER, password=NUT_PASS, debug=NUT_DEBUG, host=NUT_HOST, port=NUT_PORT )
    #nut    = PyNUT.PyNUTClient( login="upsadmin", password="upsadmin", debug=True, port=NUT_PORT )

    print( 80*"-" + "\nTesting 'GetUPSList' :")
    result = nut.GetUPSList( )
    print( "\033[01;33m%s\033[0m\n" % result )

    # [dummy]
    # driver = dummy-ups
    # desc = "Test device"
    # port = /src/nut/data/evolution500.seq
    print( 80*"-" + "\nTesting 'GetUPSVars' for 'dummy' (should be registered in upsd.conf) :")
    result = nut.GetUPSVars( "dummy" )
    print( "\033[01;33m%s\033[0m\n" % result )

    print( 80*"-" + "\nTesting 'CheckUPSAvailable' :")
    result = nut.CheckUPSAvailable( "dummy" )
    print( "\033[01;33m%s\033[0m\n" % result )

    print( 80*"-" + "\nTesting 'GetUPSCommands' :")
    result = nut.GetUPSCommands( "dummy" )
    print( "\033[01;33m%s\033[0m\n" % result )

    print( 80*"-" + "\nTesting 'GetRWVars' :")
    result = nut.GetRWVars( "dummy" )
    print( "\033[01;33m%s\033[0m\n" % result )

    print( 80*"-" + "\nTesting 'RunUPSCommand' (Test front panel) :")
    try :
        result = nut.RunUPSCommand( "UPS1", "test.panel.start" )
        if (NUT_USER is None):
            raise AssertionError("Secure operation should have failed due to lack of credentials, but did not")
    except :
        ex = str(sys.exc_info()[1])
        result = "EXCEPTION: " + ex
        if (NUT_USER is None and ex == 'ERR USERNAME-REQUIRED'):
            result = result + "\n(anticipated error: no credentials were provided)"
        else:
            if (ex != 'ERR CMD-NOT-SUPPORTED' and (NUT_USER is not None and ex != 'ERR ACCESS-DENIED') ):
                result = result + "\nTEST-CASE FAILED"
                failed.append('RunUPSCommand')
    print( "\033[01;33m%s\033[0m\n" % result )

    print( 80*"-" + "\nTesting 'SetUPSVar' (set ups.id to test):")
    try :
        result = nut.SetRWVar( "UPS1", "ups.id", "test" )
        if (NUT_USER is None):
            raise AssertionError("Secure operation should have failed due to lack of credentials, but did not")
    except :
        ex = str(sys.exc_info()[1])
        result = "EXCEPTION: " + ex
        if (NUT_USER is None and ex == 'ERR USERNAME-REQUIRED'):
            result = result + "\n(anticipated error: no credentials were provided)"
        else:
            if (ex != 'ERR VAR-NOT-SUPPORTED' and (NUT_USER is not None and ex != 'ERR ACCESS-DENIED') ):
                result = result + "\nTEST-CASE FAILED"
                failed.append('SetUPSVar')
    print( "\033[01;33m%s\033[0m\n" % result )

    # testing who has an upsmon-like log-in session to a device
    print( 80*"-" + "\nTesting 'ListClients' for 'dummy' (should be registered in upsd.conf) before test client is connected :")
    try :
        result = nut.ListClients( "dummy" )
    except :
        ex = str(sys.exc_info()[1])
        result = "EXCEPTION: " + ex
        result = result + "\nTEST-CASE FAILED"
        failed.append('ListClients-dummy-before')
    print( "\033[01;33m%s\033[0m\n" % result )

    print( 80*"-" + "\nTesting 'ListClients' for missing device (should raise an exception) :")
    try :
        result = nut.ListClients( "MissingBogusDummy" )
    except :
        ex = str(sys.exc_info()[1])
        result = "EXCEPTION: " + ex
        if (ex == 'ERR UNKNOWN-UPS'):
            result = result + "\n(anticipated error: bogus device name was tested)"
        else:
            result = result + "\nTEST-CASE FAILED"
            failed.append('ListClients-MissingBogusDummy')
    print( "\033[01;33m%s\033[0m\n" % result )

    loggedIntoDummy = False
    print( 80*"-" + "\nTesting 'DeviceLogin' for 'dummy' (should be registered in upsd.conf; current credentials should have an upsmon role in upsd.users) :")
    try :
        result = nut.DeviceLogin( "dummy" )
        if (NUT_USER is None):
            raise AssertionError("Secure operation should have failed due to lack of credentials, but did not")
        loggedIntoDummy = True
    except :
        ex = str(sys.exc_info()[1])
        result = "EXCEPTION: " + ex
        if (NUT_USER is None and ex == 'ERR USERNAME-REQUIRED'):
            result = result + "\n(anticipated error: no credentials were provided)"
        else:
            if (NUT_USER is not None and ex != 'ERR ACCESS-DENIED'):
                result = result + "\nTEST-CASE FAILED"
                failed.append('DeviceLogin-dummy')
    print( "\033[01;33m%s\033[0m\n" % result )

    print( 80*"-" + "\nTesting 'ListClients' for None (should list all devices and sessions to them, if any -- e.g. one established above) :")
    try :
        result = nut.ListClients( )
        if (type(result) is not dict):
            raise TypeError("ListClients() did not return a dict")
        else:
            if (loggedIntoDummy):
                if (len(result) < 1):
                    raise ValueError("ListClients() returned an empty dict where at least one client was expected on b'dummy'")
                if (type(result[b'dummy']) is not list):
                    raise TypeError("ListClients() did not return a dict whose b'dummy' keyed value is a list")
                if (len(result[b'dummy']) < 1):
                    raise ValueError("ListClients() returned a dict where at least one client was expected on b'dummy' but none were reported")
    except :
        ex = str(sys.exc_info()[1])
        result = "EXCEPTION: " + ex
        result = result + "\nTEST-CASE FAILED"
        failed.append('ListClients-dummy-after')
    print( "\033[01;33m%s\033[0m\n" % result )

    print( 80*"-" + "\nTesting 'PyNUT' instance teardown (end of test script)" )
    # No more tests AFTER this line; add them above the teardown message

    if (len(failed) > 0):
        print ( "SOME TEST CASES FAILED in an unexpected manner: %s" % failed )
        sys.exit(1)
