/* Device.java

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
import java.util.ArrayList;

/**
 * Class representing a device attached to a Client.
 * <p>
 * It can retrieve its description, its number of logins, its variable and command lists.
 * A Device object can be retrieved from Client instance and can not be constructed directly.
 * 
 * @author <a href="mailto:EmilienKia@eaton.com">Emilien Kia</a>
 */
public class Device {
    
    /**
     * Client to which device is attached
     */
    Client client = null;

    /**
     * Device name
     */
    String name = null;
 
    /**
     * Internally create a device.
     * @param name Device name.
     * @param client Client to which the device is attached.
     */
    protected Device(String name, Client client)
    {
        this.client      = client;
        this.name        = name;
    }

    /**
     * Return the client to which the device is connected.
     * @return Attached client.
     */
    public Client getClient() {
        return client;
    }
    
    /**
     * Return the device name.
     * @return Device name.
     */
    public String getName() {
        return name;
    }

    /**
     * Retrieve the device description from UPSD and store it in cache.
     * @return Device description
     * @throws IOException 
     */
    public String getDescription() throws IOException, NutException {
        if(client!=null)
        {
            return client.get("UPSDESC", name);
        }
        return null;
    }
    
    /**
     * Log in to the ups.
     * <p>
     * Use this to log the fact that a system is drawing power from this UPS.
     * The <i>upsmon</i> master will wait until the count of attached systems reaches
     * 1 - itself.  This allows the slaves to shut down first.
     * <p>
     * NOTE: You probably shouldn't send this command unless you are upsmon,
     * or a upsmon replacement.
     * @throws IOException
     * @throws NutException 
     */
    public void login() throws IOException, NutException {
        if(client!=null)
        {
            String res = client.query("LOGIN", name);
            if(!res.startsWith("OK"))
            {
                // Normaly response should be OK or ERR and nothing else.
                throw new NutException(NutException.UnknownResponse, "Unknown response in Device.login : " + res);
            }
        }
    }
    
    /**
     * This function doesn't do much by itself.  It is used by <i>upsmon</i> to make
     * sure that master-level functions like FSD are available if necessary
     * @throws IOException
     * @throws NutException 
     */
    public void master() throws IOException, NutException {
        if(client!=null)
        {
            String res = client.query("MASTER", name);
            if(!res.startsWith("OK"))
            {
                // Normaly response should be OK or ERR and nothing else.
                throw new NutException(NutException.UnknownResponse, "Unknown response in Device.master : " + res);
            }
        }
    }

    /**
     * Set the "forced shutdown" flag.
     * <p>
     * <i>upsmon</i> in master mode is the primary user of this function.  It sets this
     * "forced shutdown" flag on any UPS when it plans to power it off.  This is
     * done so that slave systems will know about it and shut down before the
     * power disappears.
     * <p>
     * Setting this flag makes "FSD" appear in a STATUS request for this UPS.
     * Finding "FSD" in a status request should be treated just like a "OB LB".
     * <p>
     * It should be noted that FSD is currently a latch - once set, there is  
     * no way to clear it short of restarting upsd or dropping then re-adding
     * it in the ups.conf.  This may cause issues when upsd is running on a
     * system that is not shut down due to the UPS event.
     * @throws IOException
     * @throws NutException 
     */
    public void setForcedShutdown() throws IOException, NutException {
        if(client!=null)
        {
            String res = client.query("FSD", name);
            if(!res.startsWith("OK"))
            {
                // Normaly response should be OK or ERR and nothing else.
                throw new NutException(NutException.UnknownResponse, "Unknown response in Device.setForcedShutdown : " + res);
            }
        }
    }
    
    /**
     * Return the number of clients which have done LOGIN for this UPS.
     * Force to retrieve it from UPSD and store it in cache.
     * @return Number of clients, -1 if error.
     * @throws IOException 
     */
    public int getNumLogin() throws IOException, NutException {
        if(client!=null)
        {
            String res = client.get("NUMLOGINS", name);
            return res!=null?Integer.parseInt(res):-1;
        }
        return -1;
    }
    

    /**
     * Return the list of device variables from the NUT server.
     * @return List of variables, empty if nothing,
     * null if not connected or failed.
     * @throws IOException 
     */
    public Variable[] getVariableList() throws IOException, NutException {
        if(client==null)
            return null;
        
        String[] res = client.list("VAR", name);
        if(res==null)
            return null;
        
        ArrayList/*<Variable>*/ list = new ArrayList/*<Variable>*/();
        for(int i=0; i<res.length; i++)
        {
            String[] arr = Client.splitNameValueString(res[i]);
            if(arr!=null)
            {
                list.add(new Variable(arr[0], this));
            }
        }
        return (Variable[])list.toArray(new Variable[list.size()]);
    }
    
    /**
     * Return the list of device RW variables from the NUT server.
     * @return List of variables, empty if nothing,
     * null if not connected or failed.
     * @throws IOException 
     */
    public Variable[] getRWVariableList() throws IOException, NutException {
        if(client==null)
            return null;
        
        String[] res = client.list("RW", name);
        if(res==null)
            return null;
        
        ArrayList/*<Variable>*/ list = new ArrayList/*<Variable>*/();
        for(int i=0; i<res.length; i++)
        {
            String[] arr = Client.splitNameValueString(res[i]);
            if(arr!=null)
            {
                list.add(new Variable(arr[0], this));
            }
        }
        return (Variable[])list.toArray(new Variable[list.size()]);
    }
    
    /**
     * Return a variable from its name.
     * @param name Name of the queried variable.
     * @return The corresponding variable object if exists.
     * @throws IOException
     * @throws NutException 
     */
    public Variable getVariable(String name) throws IOException, NutException {
        if(client==null)
            return null;
        
        String[] params = {this.name, name};
        client.get("VAR", params);
        return new Variable(name, this);
    }
    
    /**
     * Return the list of device commands from the NUT server.
     * @return List of commands, empty if nothing,
     * null if not connected or failed.
     * @throws IOException 
     */
    public Command[] getCommandList() throws IOException, NutException {
        if(client==null)
            return null;
        
        String[] res = client.list("CMD", name);
        if(res==null)
            return null;
        
        ArrayList/*<Command>*/ list = new ArrayList/*<Command>*/();
        for(int i=0; i<res.length; i++)
        {
            list.add(new Command(res[i], this));
        }
        return (Command[])list.toArray(new Command[list.size()]);
    }

    /**
     * Return a command from its name.
     * @param name Name of the queried command.
     * @return The corresponding command object if exists.
     * @throws IOException
     * @throws NutException 
     */
    public Command getCommand(String name)throws IOException, NutException {
        if(client==null)
            return null;
        
        String[] params = {this.name, name};
        String res = client.get("CMDDESC", params);
        // Note: there is no way to test if the command is really available or not
        // because a GET CMDDESC ups badcmdname does not return an error.
        return new Command(name, this);
    }
}
