/* NutRestProvider.java

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
package org.networkupstools.jnutwebapi;

import java.io.IOException;
import java.net.UnknownHostException;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.ws.rs.GET;
import javax.ws.rs.Path;
import javax.ws.rs.PathParam;
import javax.ws.rs.Produces;

import org.networkupstools.jnut.Client;
import org.networkupstools.jnut.Device;
import org.networkupstools.jnut.NutException;
import org.networkupstools.jnut.Variable;


/**
 * Apache Wink WS REST provider for UPSD communication.
 * @author <a href="mailto:EmilienKia@eaton.com">Emilien Kia</a>
 */
@Path("/servers")
public class NutRestProvider {

    @GET
    public String get() {
        return "UPSD connections";
    }
    
    @Path("{server}")
    public Server getServers(@PathParam("server") String server) {
        try {
            return new Server(server);
        } catch(Exception ex) {
             Logger.getLogger(NutRestProvider.class.getName()).log(Level.SEVERE, null, ex);
             return null;
        }
    }

    public class Server {
        Client client = new Client();
        
        public Server(String server) throws IOException, UnknownHostException, NutException {
            int idx = server.indexOf(':');
            if(idx!=-1)
            {
                int port = Integer.parseInt(server.substring(idx+1));
                client.connect(server.substring(0, idx), port);
            }
            else
            {
                client.connect(server, 3493);
            }
        }
        
        @GET
        @Produces("application/json")
        public String getDeviceList() {
            try {
                String str = "";
                Device[] devs = client.getDeviceList();
                boolean first = true;
                for (Device device : devs) {
                    if(first)
                        first = false;
                    else
                        str += ",";
                    str += "\n\"" + device.getName() + "\"";
                }
                return "[" + str + "\n]";
            } catch(Exception ex) {
             Logger.getLogger(NutRestProvider.class.getName()).log(Level.SEVERE, null, ex);
             return null;
            }
        }
        
        
        @Path("{dev}")
        public Dev getDev(@PathParam("dev") String dev) {
            try {
                return new Dev(dev);
            } catch(Exception ex) {
                 Logger.getLogger(NutRestProvider.class.getName()).log(Level.SEVERE, null, ex);
                 return null;
            }
        }
        
        
        
        public class Dev {
            
            Device device;
            
            public Dev(String dev) throws IOException, NutException{
                device = client.getDevice(dev);
            }

            @GET
            @Produces("application/json")
            public String getDescriptionShortcut() {
                return getDescription();
            }
            
            @GET
            @Produces("application/json")
            @Path("description")
            public String getDescription() {
                try {
                    String res = device.getDescription();
                    if(!res.startsWith("\"") && !res.endsWith("\""))
                        res = "\"" + res + "\"";
                    return res;
                } catch (Exception ex) {
                    Logger.getLogger(NutRestProvider.class.getName()).log(Level.SEVERE, null, ex);
                    return null;
                }
            }
            
            
            @GET
            @Produces("application/json")
            @Path("vars")
            public String getVars() {
                try {
                    String str = "";
                    boolean first = true;
                    for (Variable variable : device.getVariableList()) {
                        if(first)
                            first = false;
                        else
                            str += ",";
                        str += "\n\"" + variable.getName() + "\"";
                    }
                    return "[" + str + "\n]";
                } catch (Exception ex) {
                    Logger.getLogger(NutRestProvider.class.getName()).log(Level.SEVERE, null, ex);
                    return null;
                }
            }
            
            @Path("vars/{var}")
            public Var getVar(@PathParam("var") String var){
                try {
                    return new Var(var);
                } catch(Exception ex) {
                     Logger.getLogger(NutRestProvider.class.getName()).log(Level.SEVERE, null, ex);
                     return null;
                }                
            }
            
            public class Var {
                
                Variable variable = null;
                
                public Var(String var) throws IOException, NutException{
                    variable = device.getVariable(var);
                }
                
                @GET
                @Produces("application/json")
                public String getValue() {
                    try {
                        return "\"" + variable.getValue() + "\"";
                    } catch(Exception ex) {
                         Logger.getLogger(NutRestProvider.class.getName()).log(Level.SEVERE, null, ex);
                         return null;
                    }
                }
                
                @GET
                @Produces("application/json")
                @Path("description")
                public String getDescription() {
                    try {
                        String res = variable.getDescription();
                        if(!res.startsWith("\"") && !res.endsWith("\""))
                            res = "\"" + res + "\"";
                        return res;
                    } catch(Exception ex) {
                         Logger.getLogger(NutRestProvider.class.getName()).log(Level.SEVERE, null, ex);
                         return null;
                    }
                }
            }
        }
    }
}
