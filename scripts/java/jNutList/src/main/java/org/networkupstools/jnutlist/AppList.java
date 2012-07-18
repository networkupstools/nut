/* AppList.java

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
package org.networkupstools.jnutlist;

import java.io.IOException;
import java.net.UnknownHostException;
import org.networkupstools.jnut.*;


public class AppList 
{
    
    public static void main( String[] args )
    {
        String host  = args.length>=1?args[0]:"localhost";
        int    port  = args.length>=2?Integer.valueOf(args[1]).intValue():3493;
        String login = args.length>=3?args[2]:"";
        String pass  = args.length>=4?args[3]:"";
        
        System.out.println( "jNutList connecting to " + login+":"+pass+"@"+host+":"+port );
        
        Client client = new Client();
        try {
            client.connect(host, port, login, pass);
            Device[] devs = client.getDeviceList();
            if(devs!=null)
            {
                for(int d=0; d<devs.length; d++)
                {
                    Device dev = devs[d];
                    String desc = "";
                    try {
                        desc = " : " + dev.getDescription();
                    } catch(NutException e) {
                        e.printStackTrace();
                    }
                    System.out.println("DEV " + dev.getName() + desc);
                    
                    try {
                        Variable[] vars = dev.getVariableList();
                        if(vars!=null)
                        {
                            if(vars.length==0)
                                System.out.println("  NO VAR");
                            for(int v=0; v<vars.length; v++)
                            {
                                Variable var = vars[v];
                                String res = "";
                                try {
                                    res = " = " + var.getValue() + " (" + var.getDescription() + ")";
                                } catch(NutException e) {
                                    e.printStackTrace();
                                }
                                System.out.println("  VAR " + var.getName() + res );
                            }
                        }
                        else
                            System.out.println("  NULL VAR");
                    } catch(NutException e) {
                        e.printStackTrace();
                    }
                    
                    try {
                        Command[] cmds = dev.getCommandList();
                        if(cmds!=null)
                        {
                            if(cmds.length==0)
                                System.out.println("  NO CMD");
                            for(int c=0; c<cmds.length; c++)
                            {
                                Command cmd = cmds[c];
                                String res = "";
                                try {
                                    res = " : " + cmd.getDescription();
                                } catch(NutException e) {
                                    e.printStackTrace();
                                }
                                System.out.println("  CMD " + cmd.getName() + res);
                            }
                        }
                        else
                            System.out.println("  NULL CMD");
                    } catch(NutException e) {
                        e.printStackTrace();
                    }                    
                }
            }
            
            client.disconnect();
            
        }catch(Exception e){
            e.printStackTrace();
        }
        
    }
}
