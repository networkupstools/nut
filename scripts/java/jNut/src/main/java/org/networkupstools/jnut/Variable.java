/* Variable.java

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

import java.io.IOException;

/**
 * Class representing a variable of a device.
 * <p>
 * It can be used to get and set its value (if possible).
 * A Variable object can be retrieved from Device instance and can not be constructed directly.
 * 
 * @author <a href="mailto:EmilienKia@eaton.com">Emilien Kia</a>
 */
public class Variable {
    /**
     * Device to which variable is attached
     */
    Device device = null;

    /**
     * Variable name
     */
    String name = null;
  
    /**
     * Internally create a variable.
     * @param name Variable name.
     * @param device Device to which the variable is attached.
     */
    protected Variable(String name, Device device)
    {
        this.device = device;
        this.name   = name;
    }
    
    /**
     * Return the device to which the variable is related.
     * @return Attached device.
     */
    public Device getDevice() {
        return device;
    }
    
    /**
     * Return the variable name.
     * @return Command name.
     */
    public String getName() {
        return name;
    }

    /**
     * Retrieve the variable value from UPSD and store it in cache.
     * @return Variable value
     * @throws IOException 
     */
    public String getValue() throws IOException, NutException {
        if(device!=null && device.getClient()!=null)
        {
            String[] params = {device.getName(), name};
            String res = device.getClient().get("VAR", params);
            return res!=null?Client.extractDoublequotedValue(res):null;
        }
        return null;
    }

    /**
     * Retrieve the variable description from UPSD and store it in cache.
     * @return Variable description
     * @throws IOException 
     */
    public String getDescription() throws IOException, NutException {
        if(device!=null && device.getClient()!=null)
        {
            String[] params = {device.getName(), name};
            String res = device.getClient().get("DESC", params);
            return res!=null?Client.extractDoublequotedValue(res):null;
        }
        return null;
    }
    
    /**
     * Set the variable value.
     * Note the new value can be applied with a little delay depending of UPSD and connection.
     * @param value New value for the variable
     * @throws IOException 
     */
    public void setValue(String value) throws IOException, NutException {
        if(device!=null && device.getClient()!=null)
        {
            String[] params = {"VAR", device.getName(),
                    name, " \"" + Client.escape(value) + "\""};
            String res = device.getClient().query("SET", params);
            if(!res.equals("OK"))
            {
                // Normaly response should be OK or ERR and nothing else.
                throw new NutException(NutException.UnknownResponse, "Unknown response in Variable.setValue : " + res);
            }
        }
    }
    
    // TODO Add query for type, enum and range values
}
