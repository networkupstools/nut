/* ScannerTest.java

   Copyright (C) 2011 Eaton

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
package org.networkupstools.jnut;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;


/**
 * Unit test for scanner.
 */
public class ScannerTest extends TestCase {
    /**
     * Create the test case
     *
     * @param testName name of the test case
     */
    public ScannerTest( String testName )
    {
        super( testName );
    }

    /**
     * @return the suite of tests being tested
     */
    public static Test suite()
    {
        return new TestSuite( ScannerTest.class );
    }

    /**
     * Scan line test.
     */
    public void testScanLineOk()
    {
        String line = "SNMP:driver=\"snmp-ups\",port=\"192.168.1.1\",desc=\"Evolution\",mibs=\"mge\",community=\"public\"";
        Scanner.DiscoveredDevice dev = Scanner.scanLine(line);
        assertNotNull("scanLine must return a DiscoveredDevice", dev);
        assertEquals("scanLine must return the correct driver", "SNMP", dev.getDriver());
        assertEquals("scanLine must return the correct driver name", "snmp-ups", dev.getProperty("driver"));
        assertEquals("scanLine must return the correct port", "192.168.1.1", dev.getProperty("port"));
    }

    // Tool function, find a string as memeber of array.
    static boolean findStringInArray(String str, String[] arr){
        if(arr!=null)
        {
            for(int i=0; i<arr.length; i++)
                if(arr[i].equals(str))
                    return true;
        }
        return false;
    }

    /**
     * Option generator usb scan type test.
     */
    public void testOptionGeneratorTypeUSBTest()
    {
        Scanner scanner = new Scanner(Scanner.SCAN_USB);
        String[] arr = scanner.generateCommandParameters();
        assertTrue("Have usb scan", findStringInArray(Scanner.PARAM_SCAN_USB, arr));
        assertFalse("Not have snmp scan", findStringInArray(Scanner.PARAM_SCAN_SNMP, arr));
        assertFalse("Not have xml scan", findStringInArray(Scanner.PARAM_SCAN_XML, arr));
        assertFalse("Not have old nut scan", findStringInArray(Scanner.PARAM_SCAN_OLDNUT, arr));
        assertFalse("Not have avahi scan", findStringInArray(Scanner.PARAM_SCAN_AVAHI, arr));
        assertFalse("Not have ipmi scan", findStringInArray(Scanner.PARAM_SCAN_IPMI, arr));
        assertFalse("Not have complete scan", findStringInArray(Scanner.PARAM_SCAN_COMPLETE, arr));
    }

    /**
     * Option generator snmp scan type test.
     */
    public void testOptionGeneratorTypeSNMPTest()
    {
        Scanner scanner = new Scanner(Scanner.SCAN_SNMP);
        String[] arr = scanner.generateCommandParameters();
        assertFalse("Not have usb scan", findStringInArray(Scanner.PARAM_SCAN_USB, arr));
        assertTrue("Have snmp scan", findStringInArray(Scanner.PARAM_SCAN_SNMP, arr));
        assertFalse("Not have xml scan", findStringInArray(Scanner.PARAM_SCAN_XML, arr));
        assertFalse("Not have old nut scan", findStringInArray(Scanner.PARAM_SCAN_OLDNUT, arr));
        assertFalse("Not have avahi scan", findStringInArray(Scanner.PARAM_SCAN_AVAHI, arr));
        assertFalse("Not have ipmi scan", findStringInArray(Scanner.PARAM_SCAN_IPMI, arr));
        assertFalse("Not have complete scan", findStringInArray(Scanner.PARAM_SCAN_COMPLETE, arr));
    }

    /**
     * Option generator xml scan type test.
     */
    public void testOptionGeneratorTypeXMLTest()
    {
        Scanner scanner = new Scanner(Scanner.SCAN_XML);
        String[] arr = scanner.generateCommandParameters();
        assertFalse("Not have usb scan", findStringInArray(Scanner.PARAM_SCAN_USB, arr));
        assertFalse("Not have snmp scan", findStringInArray(Scanner.PARAM_SCAN_SNMP, arr));
        assertTrue("Have xml scan", findStringInArray(Scanner.PARAM_SCAN_XML, arr));
        assertFalse("Not have old nut scan", findStringInArray(Scanner.PARAM_SCAN_OLDNUT, arr));
        assertFalse("Not have avahi scan", findStringInArray(Scanner.PARAM_SCAN_AVAHI, arr));
        assertFalse("Not have ipmi scan", findStringInArray(Scanner.PARAM_SCAN_IPMI, arr));
        assertFalse("Not have complete scan", findStringInArray(Scanner.PARAM_SCAN_COMPLETE, arr));
    }

    /**
     * Option generator old nut scan type test.
     */
    public void testOptionGeneratorTypeOldNutTest()
    {
        Scanner scanner = new Scanner(Scanner.SCAN_OLDNUT);
        String[] arr = scanner.generateCommandParameters();
        assertFalse("Not have usb scan", findStringInArray(Scanner.PARAM_SCAN_USB, arr));
        assertFalse("Not have snmp scan", findStringInArray(Scanner.PARAM_SCAN_SNMP, arr));
        assertFalse("Not have xml scan", findStringInArray(Scanner.PARAM_SCAN_XML, arr));
        assertTrue("Have old nut scan", findStringInArray(Scanner.PARAM_SCAN_OLDNUT, arr));
        assertFalse("Not have avahi scan", findStringInArray(Scanner.PARAM_SCAN_AVAHI, arr));
        assertFalse("Not have ipmi scan", findStringInArray(Scanner.PARAM_SCAN_IPMI, arr));
        assertFalse("Not have complete scan", findStringInArray(Scanner.PARAM_SCAN_COMPLETE, arr));
    }

    /**
     * Option generator avahi scan type test.
     */
    public void testOptionGeneratorTypeAvahiTest()
    {
        Scanner scanner = new Scanner(Scanner.SCAN_AVAHI);
        String[] arr = scanner.generateCommandParameters();
        assertFalse("Not have usb scan", findStringInArray(Scanner.PARAM_SCAN_USB, arr));
        assertFalse("Not have snmp scan", findStringInArray(Scanner.PARAM_SCAN_SNMP, arr));
        assertFalse("Not have xml scan", findStringInArray(Scanner.PARAM_SCAN_XML, arr));
        assertFalse("Not have old nut scan", findStringInArray(Scanner.PARAM_SCAN_OLDNUT, arr));
        assertTrue("Have avahi scan", findStringInArray(Scanner.PARAM_SCAN_AVAHI, arr));
        assertFalse("Not have ipmi scan", findStringInArray(Scanner.PARAM_SCAN_IPMI, arr));
        assertFalse("Not have complete scan", findStringInArray(Scanner.PARAM_SCAN_COMPLETE, arr));
    }

    /**
     * Option generator IPMI scan type test.
     */
    public void testOptionGeneratorTypeIPMITest()
    {
        Scanner scanner = new Scanner(Scanner.SCAN_IPMI);
        String[] arr = scanner.generateCommandParameters();
        assertFalse("Not have usb scan", findStringInArray(Scanner.PARAM_SCAN_USB, arr));
        assertFalse("Not have snmp scan", findStringInArray(Scanner.PARAM_SCAN_SNMP, arr));
        assertFalse("Not have xml scan", findStringInArray(Scanner.PARAM_SCAN_XML, arr));
        assertFalse("Not have old nut scan", findStringInArray(Scanner.PARAM_SCAN_OLDNUT, arr));
        assertFalse("Not have avahi scan", findStringInArray(Scanner.PARAM_SCAN_AVAHI, arr));
        assertTrue("Have ipmi scan", findStringInArray(Scanner.PARAM_SCAN_IPMI, arr));
        assertFalse("Not have complete scan", findStringInArray(Scanner.PARAM_SCAN_COMPLETE, arr));
    }

    /**
     * Option generator complete scan type test.
     */
    public void testOptionGeneratorTypeCompleteTest()
    {
        Scanner scanner = new Scanner(Scanner.SCAN_COMPLETE);
        String[] arr = scanner.generateCommandParameters();
        assertTrue("Have complete scan", findStringInArray(Scanner.PARAM_SCAN_COMPLETE, arr));
    }

}
