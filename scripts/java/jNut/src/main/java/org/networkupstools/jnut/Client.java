/* Client.java

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
import java.net.UnknownHostException;
import java.util.ArrayList;

/**
 * A jNut client is start point to dialog to UPSD.
 * It can connect to an UPSD then retrieve its device list.
 * It support authentication by login/password.
 * <p>
 * You can directly create and connect a client by using the
 * Client(String host, int port, String login, String passwd) constructor
 * or use a three phase construction:
 * <ul>
 *  <li>empty constructor
 *  <li>setting host, port, login and password with setters
 *  <li>call empty connect()
 * </ul>
 * <p>
 * Objects retrieved by Client are attached (directly or indirectly) to it.
 * If the connection is closed, attached objects must not be used anymore (GC).
 * <p>
 * Note: The jNut Client does not support any reconnection nor ping mechanism,
 * so the calling application must know the UPSD can timeout the connection.
 * <p>
 * Note: Retrieved values are not valid along the time, they are valid at the
 * precise moment they are retrieved.
 * 
 * @author <a href="mailto:EmilienKia@eaton.com">Emilien Kia</a>
 */
public class Client {
    
    /**
     * Host to which connect.
     * Network name or IP.
     * Default to "127.0.0.1"
     */
    private String host = "127.0.0.1";
    
    /**
     * IP port.
     * Default to 3493
     */
    private int    port = 3493;
    
    /**
     * Login to use to connect to UPSD.
     */
    private String login = null;

    /**
     * Password to use to connect to UPSD.
     */
    private String passwd = null;

    /**
     * Communication socket
     */
    private StringLineSocket socket = null;

    
    /**
     * Get the host name or address to which client is (or will be) connected.
     * @return Host name or address.
     */
    public String getHost() {
        return host;
    }

    /**
     * Set the host name (or address) to which the client will intend to connect to at next connection.
     * @param host New host name or address.
     */
    public void setHost(String host) {
        this.host = host;
    }

    /**
     * Get the login with which the client is (or will be connected).
     * @return The login.
     */
    public String getLogin() {
        return login;
    }

    /**
     * Set the login with which the client will intend to connect.
     * @param login New login.
     */
    public void setLogin(String login) {
        this.login = login;
    }

    /**
     * Get the password with which the client is (or will be connected).
     * @return The password.
     */
    public String getPasswd() {
        return passwd;
    }

    /**
     * Set the password with which the client will intend to connect.
     * @param passwd New password.
     */
    public void setPasswd(String passwd) {
        this.passwd = passwd;
    }

    /**
     * Get the port to which client is (or will be) connected.
     * @return Port number.
     */   
    public int getPort() {
        return port;
    }
    
    /**
     * Set the port to which client is (or will be) connected.
     * @param port Port number.
     */   
    public void setPort(int port) {
        this.port = port;
    }
    
    
    
    /**
     * Default constructor.
     */
    public Client()
    {
    }

    /**
     * Connection constructor.
     * Construct the Client object and intend to connect.
     * Throw an exception if cannot connect.
     * @param host Host to which connect.
     * @param port IP port.
     * @param login Login to use to connect to UPSD.
     * @param passwd Password to use to connect to UPSD.
     */
    public Client(String host, int port, String login, String passwd) throws IOException, UnknownHostException, NutException
    {
        connect(host, port, login, passwd);
    }
    
    /**
     * Intent to connect and authenticate to an UPSD with specified parameters.
     * Throw an exception if cannot connect.
     * @param host Host to which connect.
     * @param port IP port.
     * @param login Login to use to connect to UPSD.
     * @param passwd Password to use to connect to UPSD.
     */
    public void connect(String host, int port, String login, String passwd) throws IOException, UnknownHostException, NutException
    {
        this.host = host;
        this.port = port;
        this.login = login;
        this.passwd = passwd;
        connect();
    }
    
    /**
     * Intent to connect to an UPSD with specified parameters without authentication.
     * Throw an exception if cannot connect.
     * @param host Host to which connect.
     * @param port IP port.
     */
    public void connect(String host, int port) throws IOException, UnknownHostException, NutException
    {
        this.host = host;
        this.port = port;
        connect();
    }

    /**
     * Connection to UPSD with already specified parameters.
     * Throw an exception if cannot connect.
     */
    public void connect() throws IOException, UnknownHostException, NutException
    {
        // Force disconnect if another connection is alive.
        if(socket!=null)
            disconnect();
        
        socket = new StringLineSocket(host, port);
        
        authenticate();
    }

    /**
     * Intend to authenticate with specified login and password, overriding 
     * already defined ones.
     * @param login
     * @param passwd
     * @throws IOException
     * @throws NutException 
     */
    public void authenticate(String login, String passwd) throws IOException, NutException
    {
        this.login = login;
        this.passwd = passwd;
        authenticate();
    }
    
    /**
     * Intend to authenticate with alread set login and password.
     * @throws IOException
     * @throws NutException 
     */
    public void authenticate() throws IOException, NutException
    {
        // Send login
        if(login!=null && !login.isEmpty())
        {
            String res = query("USERNAME", login);
            if(!res.startsWith("OK"))
            {
                // Normaly response should be OK or ERR and nothing else.
                throw new NutException(NutException.UnknownResponse, "Unknown response in Client.connect (USERNAME) : " + res);
            }
        }
        // Send password
        if(passwd!=null && !passwd.isEmpty())
        {
            String res = query("PASSWORD", passwd);
            if(!res.startsWith("OK"))
            {
                // Normaly response should be OK or ERR and nothing else.
                throw new NutException(NutException.UnknownResponse, "Unknown response in Client.connect (PASSWORD) : " + res);
            }
        }
    }
    
    /**
     * Test if the client is connected to the UPSD.
     * Note: it does not detect if the connection have been closed by server.
     * @return True if connected.
     */
    public boolean isConnected()
    {
        return socket!=null && socket.isConnected();
    }
    
    /**
     * Disconnect.
     */
    public void disconnect()
    {
        if(socket!=null)
        {
            try
            {
                if(socket.isConnected())
                    socket.close();
            }
            catch(IOException e)
            {
                e.printStackTrace();
            }
            socket = null;
        }
    }
    
    /**
     * Log out.
     */
    public void logout()
    {
        if(socket!=null)
        {
            try
            {
                if(socket.isConnected())
                {
                    socket.write("LOGOUT");
                    socket.close();
                }
            }
            catch(IOException e)
            {
                e.printStackTrace();
            }
            socket = null;
        }
    }    
    
    /**
     * Merge an array of stings into on string, with a space ' ' separator.
     * @param str First string to merge
     * @param strings Additionnal strings to merge
     * @param sep Separator.
     * @return The merged string, empty if no source string.
     */
    static String merge(String str, String[] strings)
    {
        String res = str;
        if(strings!=null)
        {
            for(int n=0; n<strings.length; n++)
            {
                res += " " + strings[n];
            }
        }
        return res;
    }
    
    /**
     * Intend to split a name/value string of the form '<name> "<value>"'.
     * @param source String source to split.
     * @return String couple with name and value.
     */
    static String[] splitNameValueString(String source)
    {
        int pos = source.indexOf(' ');
        if(pos<1)
            return null;
        String name  = source.substring(0, pos);
        String value = extractDoublequotedValue(source.substring(pos+1));
        if(value==null)
            return null;
        String[] res = new String[2];
        res[0] = name;
        res[1] = value;
        return res;
    }
    
    /**
     * Intend to extract a value from its doublequoted and escaped representation.
     * @param source Source string to convert.
     * @return Extracted value
     */
    static String extractDoublequotedValue(String source)
    {
        // Test doublequote at begin and end of string, then remove them.
        if(!(source.startsWith("\"") && source.endsWith("\"")))
            return null;
        source = source.substring(1, source.length()-1);
        // Unescape it.
        return unescape(source);
    }
    
    /**
     * Escape string with backslashes.
     * @param str String to escape.
     * @return Escaped string.
     */
    static String escape(String str)
    {
        // Replace a backslash by two backslash (regexp)
        str = str.replaceAll("\\\\", "\\\\\\\\");
        // Replace a doublequote by backslash-doublequote (regexp)
        str = str.replaceAll("\"", "\\\\\"");
        return str;
    }

    /**
     * Unescape string with backslashes.
     * @param str String to unescape.
     * @return Unescaped string.
     */
    static String unescape(String str)
    {
        // Replace a backslash-doublequote by doublequote (regexp)
        str = str.replaceAll("\\\\\"", "\"");
        // Replace two backslash by a backslash (regexp)
        str = str.replaceAll("\\\\\\\\", "\\\\");
        return str;
    }

    /**
     * Detect an UPSD ERR line.
     * If found, parse it, construct and throw an NutException
     * @param str Line to analyse.
     * @throws NutException
     */
    private void detectError(String str) throws NutException
    {
        if(str.startsWith("ERR "))
        {
            String[] arr = str.split(" ", 3);
            switch(arr.length)
            {
                case 2:
                    throw new NutException(arr[1]);
                case 3:
                    throw new NutException(arr[1], arr[2]);
                default:
                    throw new NutException();
            }
        }        
    }
    
    /**
     * Send a query line then read the response.
     * Helper around query(String).
     * @param query Query to send.
     * @param subquery Sub query to send.
     * @return The reply.
     * @throws IOException 
     */
    protected String query(String query, String subquery) throws IOException, NutException
    {
        return query(query + " " + subquery);
    }
    
    /**
     * Send a query line then read the response.
     * Helper around query(String, String ...).
     * @param query Query to send.
     * @param subquery Sub query to send.
     * @param params Optionnal additionnal parameters.
     * @return The reply.
     * @throws IOException 
     */
    protected String query(String query, String subquery, String[] params) throws IOException, NutException
    {
        return query(query + " " + subquery, params);
    }
    
    /**
     * Send a query line then read the response.
     * @param query Query to send.
     * @param params Optionnal additionnal parameters.
     * @return The reply.
     * @throws IOException 
     */
    protected String query(String query, String [] params) throws IOException, NutException
    {
        query = merge(query, params);
        return query(query);
    }

    /**
     * Send a query line then read the response.
     * @param query Query to send.
     * @return The reply.
     * @throws IOException 
     */
    protected String query(String query) throws IOException, NutException
    {
        if(!isConnected())
            return null;
        
        socket.write(query);
        String res = socket.read();
        detectError(res);
        return res;
    }   
    
    /**
     * Send a GET query line then read the reply and validate the response.
     * @param subcmd GET subcommand to send.
     * @param param Extra parameters
     * @return GET result return by UPSD, without the subcommand and param prefix.
     * @throws IOException 
     */
    protected String get(String subcmd, String param) throws IOException, NutException
    {
        String[] params = {param};
        return get(subcmd, params);
    }
    
    /**
     * Send a GET query line then read the reply and validate the response.
     * @param subcmd GET subcommand to send.
     * @param params Eventual extra parameters.
     * @return GET result return by UPSD, without the subcommand and param prefix.
     * @throws IOException 
     */
    protected String get(String subcmd, String [] params) throws IOException, NutException
    {
        if(!isConnected())
            return null;
        
        subcmd = merge(subcmd, params);
        socket.write("GET " + subcmd);
        String res = socket.read();
        if(res==null)
            return null;
        detectError(res);
        if(res.startsWith(subcmd + " "))
        {
            return res.substring(subcmd.length()+1);
        }
        else
        {
            return null;
        }
    }

    /**
     * Send a LIST query line then read replies and validate them.
     * @param subcmd LIST subcommand to send.
     * @return LIST results return by UPSD, without the subcommand and param prefix.
     * @throws IOException 
     */
    protected String[] list(String subcmd) throws IOException, NutException
    {
        return list(subcmd, (String[])null);
    }
    
    /**
     * Send a LIST query line then read replies and validate them.
     * @param subcmd LIST subcommand to send.
     * @param param Extra parameters.
     * @return LIST results return by UPSD, without the subcommand and param prefix.
     * @throws IOException 
     */
    protected String[] list(String subcmd, String param) throws IOException, NutException
    {
        String[] params = {param};
        return list(subcmd, params);        
    }
    
    /**
     * Send a LIST query line then read replies and validate them.
     * @param subcmd LIST subcommand to send.
     * @param params Eventual extra parameters.
     * @return LIST results return by UPSD, without the subcommand and param prefix.
     * @throws IOException 
     */
    protected String[] list(String subcmd, String [] params) throws IOException, NutException
    {
        if(!isConnected())
            return null;
        
        subcmd = merge(subcmd, params);
        socket.write("LIST " + subcmd);
        String res = socket.read();
        if(res==null)
            return null;
        detectError(res);
        if(!res.startsWith("BEGIN LIST " + subcmd))
            return null;
        
        ArrayList/*<String>*/ list = new ArrayList/*<String>*/();
        int sz = subcmd.length()+1;
        while(true)
        {
            res = socket.read();
            detectError(res);
            if(!res.startsWith(subcmd + " "))
                break;
            list.add(res.substring(sz));
        }
        if(!res.equals("END LIST " + subcmd))
            return null;
        
        return (String[])list.toArray(new String[list.size()]);
    }
    
    
    /**
     * Returns the list of available devices from the NUT server.
     * @return List of devices, empty if nothing,
     * null if not connected or failed.
     * 
     */
    public Device[] getDeviceList() throws IOException, NutException
    {
        String[] res = list("UPS");
        if(res==null)
            return null;

        ArrayList/*<Device>*/ list = new ArrayList/*<Device>*/();
        for(int i=0; i<res.length; i++)
        {
            String[] arr = splitNameValueString(res[i]);
            if(arr!=null)
            {
                list.add(new Device(arr[0], this));
            }
        }
        return (Device[])list.toArray(new Device[list.size()]);
    }

    /**
     * Intend to retrieve a device by its name.
     * @param name Name of the device to look at.
     * @return Device 
     * @throws IOException
     * @throws NutException 
     */
    public Device getDevice(String name)throws IOException, NutException
    {
        // Note: an exception "DRIVER-NOT-CONNECT" should not prevent Device creation.
        try{
            get("UPSDESC", name);
        }catch(NutException ex){
            if(!ex.is(NutException.DriverNotConnected)){
                throw ex;
            }
        }
        return new Device(name, this);
    }
}
