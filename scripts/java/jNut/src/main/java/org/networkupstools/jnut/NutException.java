/* NutException.java

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

/**
 * Class representing a NUT exception.
 * <p>
 * Instance are thrown when an UPSD returns an error with an "ERR" directive.
 * Moreover it can ben thrown with some extra errors like:
 * <ul>
 *  <li>UNKNOWN-RESPONSE : The response is not understood
 * </ul>
 * <p>
 * A Nut exception has a (standard java exception message) message which correspond
 * to error code returns by UPSD (like 'ACCESS-DENIED', 'UNKNOWN-UPS' ...).
 * An extra string embed a more descriptive english message.
 * 
 * @author <a href="mailto:EmilienKia@eaton.com">Emilien Kia</a>
 */
public class NutException extends java.lang.Exception{

    public static String UnknownResponse = "UNKNOWN-RESPONSE";
    
    public static String DriverNotConnected = "DRIVER-NOT-CONNECTED";
    
    public String extra = "";

    public NutException()
    {
    }
    
    public NutException(String message)
    {
        super(message);
    }

    public NutException(String message, String extra)
    {
        super(message);
        this.extra = extra;
    }

    public NutException(Throwable cause)
    {
        super(cause);
    }
    
    public NutException(String message, Throwable cause)
    {
        super(message, cause);
    }

    public NutException(String message, String extra, Throwable cause)
    {
        super(message, cause);
        this.extra = extra;
    }

    /**
     * Returns the extra message.
     * @return Extra message if any.
     */
    public String getExtra() {
        return extra;
    }

    /**
     * Set the extra message.
     * @param extra The new extra message.
     */
    public void setExtra(String extra) {
        this.extra = extra;
    }
    
    /**
     * Test is the exception corresponds to the specified name.
     * @param name Name to test
     * @return True if exception corresponds.
     */
    public boolean is(String name) {
        return getMessage()!=null&&getMessage().equals(name);
    }

    /**
     * Format an exception message.
     * @return Exception message
     */
    public String toString() {
        return "[" + getClass().getSimpleName() + "]" + getMessage() + " : " + getExtra();
    }
}
