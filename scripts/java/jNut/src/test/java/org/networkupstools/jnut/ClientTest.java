/* ClientTest.java

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
 * Unit test for simple App.
 */
public class ClientTest extends TestCase
{
    /**
     * Create the test case
     *
     * @param testName name of the test case
     */
    public ClientTest( String testName )
    {
        super( testName );
    }

    /**
     * @return the suite of tests being tested
     */
    public static Test suite()
    {
        return new TestSuite( ClientTest.class );
    }

    /**
     * Escape function test.
     */
    public void testEscape()
    {
        assertEquals("Empty string", "", Client.escape(""));
        assertEquals("Simple string", "hello", Client.escape("hello"));
        assertEquals("Internal doublequote", "he\\\"llo", Client.escape("he\"llo"));
        assertEquals("Internal backslash", "he\\\\llo", Client.escape("he\\llo"));
        assertEquals("Internal backslash and doublequote", "he\\\\\\\"llo", Client.escape("he\\\"llo"));
        assertEquals("Initial and final doublequote", "\\\"hello\\\"", Client.escape("\"hello\""));
    }
    
    /**
     * Unescape function test.
     */
    public void testUnescape()
    {
        assertEquals("Empty string", "", Client.unescape(""));
        assertEquals("Simple string", "hello", Client.unescape("hello"));
        assertEquals("Internal doublequote", "he\"llo", Client.unescape("he\\\"llo"));
        assertEquals("Internal backslash", "he\\llo", Client.unescape("he\\\\llo"));
        assertEquals("Internal backslash and doublequote", "he\\\"llo", Client.unescape("he\\\\\\\"llo"));
        assertEquals("Initial and final doublequote", "\"hello\"", Client.unescape("\\\"hello\\\""));
    }
    
    /**
     * extractDoublequotedValue function test.
     */
    public void testExtractDoublequotedValue()
    {
        assertNull("Empty string", Client.extractDoublequotedValue(""));
        assertNull("Non doublequoted string", Client.extractDoublequotedValue("hello"));
        assertNull("No begining doublequote", Client.extractDoublequotedValue("hello\""));
        assertNull("No ending doublequote", Client.extractDoublequotedValue("\"hello"));
        assertEquals("Simple string", "hello", Client.extractDoublequotedValue("\"hello\""));
        assertEquals("String with doublequote", "he\"llo", Client.extractDoublequotedValue("\"he\\\"llo\""));
        assertEquals("String with backslash", "he\\llo", Client.extractDoublequotedValue("\"he\\\\llo\""));
        assertEquals("String with backslash and doublequote", "he\\\"llo", Client.extractDoublequotedValue("\"he\\\\\\\"llo\""));
    }
    
    /**
     * splitNameValueString function test.
     */
    public void testSplitNameValueString()
    {
        String[] res;
        assertNull("Empty string", Client.splitNameValueString(""));
        assertNull("One word string", Client.splitNameValueString("name"));
        assertNull("Non doublequoted string", Client.extractDoublequotedValue("name value"));
        assertNull("No begining doublequote", Client.extractDoublequotedValue("name value\""));
        assertNull("No ending doublequote", Client.extractDoublequotedValue("name \"value"));
        res = Client.splitNameValueString("name \"value\"");
        assertEquals("Simple name/value (name)", "name", res[0]);
        assertEquals("Simple name/value (value)", "value", res[1]);
        res = Client.splitNameValueString("name \"complex value\"");
        assertEquals("Simple name / complex value (name)", "name", res[0]);
        assertEquals("Simple name / complex value (value)", "complex value", res[1]);
        res = Client.splitNameValueString("name \"complex\\\\value\"");
        assertEquals("Simple name / backslash value (name)", "name", res[0]);
        assertEquals("Simple name / backslash value (value)", "complex\\value", res[1]);        
        res = Client.splitNameValueString("name \"complex\\\"value\"");
        assertEquals("Simple name / doublequote value (name)", "name", res[0]);
        assertEquals("Simple name / doublequote value (value)", "complex\"value", res[1]);        
    }
}
