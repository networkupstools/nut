/* Scanner.java

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

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * jNut scanner.
 * Wrap calls to nut-scanner in java.
 *
 * Just, instantiate it, set options and nut-scanner path, then scan.
 * <pre>
 * Scanner scanner = new Scanner();
 * scanner.setExecName("/usr/local/ups/bin/nut-scanner");
 * scanner.setExecPath("/usr/local/ups/bin");
 * scanner.setParam(Scanner.OPTION_MASK_CIDR, "192.168.1.1/24");
 * Scanner.DiscoveredDevice[] devs = scanner.scan();
 * </pre>
 *
 * @author <a href="mailto:EmilienKia@eaton.com">Emilien Kia</a>
 */
public class Scanner {

    public static final int SCAN_USB = 1 /*<< 0*/;
    public static final int SCAN_SNMP = 1 << 1;
    public static final int SCAN_XML = 1 << 2;
    public static final int SCAN_OLDNUT = 1 << 3;
    public static final int SCAN_AVAHI = 1 << 4;
    public static final int SCAN_IPMI = 1 << 5;
    public static final int SCAN_COMPLETE = -1;

    public static final String OPTION_SCANNER_EXEC = "scanner_exec";
    public static final String OPTION_SCANNER_PATH = "scanner_path";
    public static final String OPTION_TIMEOUT = "timeout";
    public static final String OPTION_START_IP = "start_ip";
    public static final String OPTION_END_IP = "end_ip";
    public static final String OPTION_MASK_CIDR = "mask_cidr";
    public static final String OPTION_SNMPv1_COMMUNITY = "community";
    public static final String OPTION_SNMPv3_SECURITY_LEVEL = "secLevel";
    public static final String OPTION_SNMPv3_SECURITY_NAME = "secName";
    public static final String OPTION_SNMPv3_AUTHENTICATION_PROTOCOL = "authProtocol";
    public static final String OPTION_SNMPv3_AUTHENTICATION_PASSWORD = "authPassword";
    public static final String OPTION_SNMPv3_PRIVACY_PROTOCOL = "privProtocol";
    public static final String OPTION_SNMPv3_PRIVACY_PASSWORD = "privPassword";
    public static final String OPTION_NUT_UPSD_PORT = "port";

    /**
     * Result of a scan.
     * @see Scanner#scan()
     * Used to retrieve informations about devices found by a scan and
     * their properties
     */
    public static class DiscoveredDevice {

        String driver = null;
        Map/*<String,String>*/ properties = new HashMap/*<String,String>*/();

        /**
         * Constructor.
         * Can not be used by others than Scanner.
         * @param drv Driver
         * @param props Extra properties
         */
        DiscoveredDevice(String drv, Map props) {
            driver = drv;
            if (props != null) {
                properties = props;
            }
        }

        /**
         * Retrieve the name of driver used by device.
         * @return Driver name
         */
        public String getDriver() {
            return driver;
        }

        /**
         * Retrieve the map of device properties.
         * @return Device property map
         */
        public Map/*<String,String>*/ getProperties() {
            return properties;
        }

        /**
         * Retrieve a device property if exists.
         * @param name Name of the property to retrieve.
         * @return Property value or null if not set.
         */
        public String getProperty(String name) {
            return (String) properties.get(name);
        }

        /**
         * Test if the device has a property.
         * @param name Name of the property.
         * @return True if the property is set.
         */
        public boolean hasProperty(String name) {
            return properties.containsKey(name);
        }
    }

    static final String NUT_SCANNNER_EXEC_NAME = "nut-scanner";
    static final String PARAM_PARSABLE = "-P";
    static final String PARAM_QUIET = "-q";
    static final String PARAM_SCAN_USB = "-U";
    static final String PARAM_SCAN_SNMP = "-S";
    static final String PARAM_SCAN_XML = "-M";
    static final String PARAM_SCAN_OLDNUT = "-O";
    static final String PARAM_SCAN_AVAHI = "-A";
    static final String PARAM_SCAN_IPMI = "-I";
    static final String PARAM_SCAN_COMPLETE = "-C";
    static final String PARAM_TIMEOUT = "-t";
    static final String PARAM_START_IP = "-s";
    static final String PARAM_END_IP = "-e";
    static final String PARAM_MASK_CIDR = "-m";
    static final String PARAM_SNMPv1_COMMUNITY = "-c";
    static final String PARAM_SNMPv3_SECURITY_LEVEL = "-l";
    static final String PARAM_SNMPv3_SECURITY_NAME = "-u";
    static final String PARAM_SNMPv3_AUTHENTICATION_PROTOCOL = "-a";
    static final String PARAM_SNMPv3_AUTHENTICATION_PASSWORD = "-A";
    static final String PARAM_SNMPv3_PRIVACY_PROTOCOL = "-x";
    static final String PARAM_SNMPv3_PRIVACY_PASSWORD = "-X";
    static final String PARAM_NUT_UPSD_PORT = "-p";
    static final String[] PARAM_COUPLE_DEFINITION = {
        OPTION_TIMEOUT, PARAM_TIMEOUT,
        OPTION_START_IP, PARAM_START_IP,
        OPTION_END_IP, PARAM_END_IP,
        OPTION_MASK_CIDR, PARAM_MASK_CIDR,
        OPTION_SNMPv1_COMMUNITY, PARAM_SNMPv1_COMMUNITY,
        OPTION_SNMPv3_SECURITY_LEVEL, PARAM_SNMPv3_SECURITY_LEVEL,
        OPTION_SNMPv3_SECURITY_NAME, PARAM_SNMPv3_SECURITY_NAME,
        OPTION_SNMPv3_AUTHENTICATION_PROTOCOL, PARAM_SNMPv3_AUTHENTICATION_PROTOCOL,
        OPTION_SNMPv3_AUTHENTICATION_PASSWORD, PARAM_SNMPv3_AUTHENTICATION_PASSWORD,
        OPTION_SNMPv3_PRIVACY_PROTOCOL, PARAM_SNMPv3_PRIVACY_PROTOCOL,
        OPTION_SNMPv3_PRIVACY_PASSWORD, PARAM_SNMPv3_PRIVACY_PASSWORD,
        OPTION_NUT_UPSD_PORT, PARAM_NUT_UPSD_PORT
    };

    /** Type of scan, union of SCAN_* flags. */
    int scanType = SCAN_COMPLETE;
    /** Map of scan parameters. */
    Map/*<String,String>*/ config = null;

    /**
     * Default constructor (scan for all device types).
     */
    public Scanner() {
    }

    /**
     * Constructor with device types.
     * Construct a scanner object with specifying what type of scan to do.
     * @param scanType Type of scan, union of SCAN_* flags.
     */
    public Scanner(int scanType) {
        this.scanType = scanType;
    }

    /**
     * Constructor with device types and scan parameters.
     * Construct a scanner object with specifying what type of scan to do and
     * extra scan parameters.
     * @param scanType Type of scan, union of SCAN_* flags.
     * @param config Map of extra parameters, names are OPTION_*.
     */
    public Scanner(int scanType, Map/*<String,String>*/ config) {
        this.scanType = scanType;
        this.config = config;
    }

    /**
     * Retrieve nut-scanner executable name (with location if any).
     * Default to "nut-scanner".
     * @return nut-scanner executable name
     */
    public String getExecName() {
        return config!=null?(String)config.get(OPTION_SCANNER_EXEC):null;
    }

    /**
     * Set nut-scanner executable name (with location if any).
     * @param value nut-scanner executable name
     */
    public void setExecName(String value) {
        setParam(OPTION_SCANNER_EXEC, value);
    }

    /**
     * Retrieve nut-scanner executable path.
     * The directory in which nut-scanner will be launched.
     * @return nut-scanner executable path
     */
    public String getExecPath() {
        return config!=null?(String)config.get(OPTION_SCANNER_PATH):null;
    }

    /**
     * Set nut-scanner executable path.
     * The directory in which nut-scanner will be launched.
     * @param value nut-scanner executable path
     */
    public void setExecPath(String value) {
        setParam(OPTION_SCANNER_PATH, value);
    }

    /**
     * Retrieve scanner extra parameters like snmp community name or passwords.
     * @return Map of parameters.
     */
    public Map getConfig() {
        return config;
    }

    /**
     * Set the scanner extra parameters.
     * @param map Map of parameters.
     */
    public void setConfig(Map config) {
        this.config = config;
    }

    /**
     * Set a scanner extra parameter.
     * @param name Name of the parameter.
     * @param value Value of the parameter.
     */
    public void setParam(String name, String value) {
        if (config == null) {
            config = new HashMap/*<String,String>*/();
        }
        config.put(name, value);
    }

    /**
     * Remove a scanner extra parameter.
     * @param name Name of the parameter to remove.
     */
    public void removeParam(String name) {
        if (config != null) {
            config.remove(name);
        }
    }

    /**
     * Retrieve a scanner extra parameter.
     * @param name Name of the parameter.
     * @return Value of the parameter, null if not found.
     */
    public String getParam(String name) {
        if (config == null) {
            return (String)config.get(name);
        }
        return null;
    }

    /**
     * Test if a scanner has an extra parameter.
     * @param name Name of the parameter.
     * @return True if the scanner has the parameter set.
     */
    public boolean hasParam(String name) {
        if (config == null) {
            return config.get(name)!=null;
        }
        return false;
    }

    /**
     * Retrieve the scan type.
     * @return Union of SCAN_* flags.
     */
    public int getScanType() {
        return scanType;
    }

    /**
     * Set the scan type.
     * @param scanType Union of SCAN_* flags.
     */
    public void setScanType(int scanType) {
        this.scanType = scanType;
    }

    /**
     * Execute the scan.
     * @return Array of found DiscoveredDevice, null if a problem occurs,
     * empty if no one found.
     * @throws IOException
     */
    public DiscoveredDevice[] scan() throws IOException {
        // Launch the scanner in a slave process and parse the stdout result.
        String[] params = generateCommandParameters();
        Runtime runtime = Runtime.getRuntime();
        Process process = null;
        if (params != null && params.length > 0 && runtime != null) {
            String localExecPath = getExecPath();
            File dir = null;

            if (localExecPath != null && !localExecPath.isEmpty()) {
                dir = new File(localExecPath);
                if (!dir.exists() || !dir.isDirectory()) {
                    dir = null;
                }
            }
            if (dir != null) {
                process = runtime.exec(params, null, dir);
            } else {
                process = runtime.exec(params);
            }
        }
        if (process != null) {
            DiscoveredDevice[] res = processScanResult(process);
            try{
                if(process.exitValue()==0)
                    return res;
            }catch(IllegalThreadStateException e){
                return res;
            }
        }
        return null;
    }

    /**
     * Parse the result produced by the nut-scanner process and return it.
     * @param process Process of nut-scanner.
     * @return Array of found DiscoveredDevice, null if a problem occurs,
     * empty if no one found
     * @throws IOException
     */
    DiscoveredDevice[] processScanResult(Process process) throws IOException {
        List/*<DiscoveredDevice>*/ list = new ArrayList/*<DiscoveredDevice>*/();
        InputStream is = process.getInputStream();
        BufferedReader in = new BufferedReader(new InputStreamReader(is));
        String line;
        for (line = in.readLine(); line != null; line = in.readLine()) {
            DiscoveredDevice dev = scanLine(line);
            if (dev != null) {
                list.add(dev);
            }
        }
        return (DiscoveredDevice[]) list.toArray(new DiscoveredDevice[list.size()]);
    }

    /**
     * Parse a line of result from nut-scanner and convert it to DiscoveredDevice.
     * @param line Line of nut-scanner result.
     * @return The corresponding DiscoveredDevice, or null if a problem occurs.
     */
    static DiscoveredDevice scanLine(String line) {
        String driver = null;
        Map conf = new HashMap/*<String,String>*/();


        // Find driver name
        int pos = line.indexOf(':');
        if (pos <= 0) {
            // No driver name, it
            return null;
        }
        driver = line.substring(0, pos);
        pos++;

        // Process key[=value] pairs with Finished State Machine
        int state = 0;
        String temp = "";
        String key = "";
        char oldc = 0;
        boolean isSpecial = false;
        while (pos < line.length()) {
            char c = line.charAt(pos++);
            switch (state) {
                case 0: // Initial, key content
                {
                    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_')) {
                        temp += c;
                    } else if (c == '=') {
                        // Find the '=' key/value separator
                        if (temp.length() > 0) {
                            key = temp;
                            temp = "";
                            state = 1;
                        }else{
                            // Bad format
                            return null;
                        }
                    } else if (c == ',') {
                        // Find the ',' key/value separator (no value, key only)
                        conf.put(temp, null);
                        temp = "";
                    }else{
                        // Bad format
                        return null;
                    }
                    break;
                }
                case 1: // start value content
                {
                    isSpecial = false;
                    if (c == '"') {
                        state = 2; // Quoted value
                    } else {
                        temp += c;
                        state = 3; // Unquoted value
                    }
                    break;
                }
                case 2: // Quoted value
                {
                    if (isSpecial) {
                        temp += "\\";
                        isSpecial = false;
                        temp += c;
                    } else if (c == '\\') {
                        isSpecial = true;
                    } else if (c == '"') {
                        conf.put(key, Client.unescape(temp));
                        key = temp = "";
                        state = 4; // Wait for a ',' separator
                    } else {
                        temp += c;
                    }
                    break;
                }
                case 3: // Unquoted value
                {
                    if (c != ',') {
                        temp += c;
                    } else {
                        conf.put(key, temp);
                        key = temp = "";
                        state = 0; // Wait for a new key
                    }
                    break;
                }
                case 4: // Wait for ',' separator
                {
                    if (c == ',') {
                        state = 0;
                    } else {
                        // Bad format
                        return null;
                    }
                    break;
                }
                default: //Out of known states, must not occurs
                {
                    // Bad format
                    return null;
                }
            }
        }
        // Out of FSM, end of line
        switch (state) {
            case 0: // Initial, key content
            {
                if (temp.length() > 0) {
                    conf.put(temp, null); // Add the last key-only key/value member
                }
                break;
            }
            case 1: // start value content
            {
                conf.put(temp, ""); // Add the last key/value with empty (not null) value
                break;
            }
            case 2: // Quoted value
            {
                // should not occurs : not closed quoted value
                conf.put(key, Client.unescape(temp));
                break;
            }
            case 3: // Unquoted value
            {
                // Correct end of string, store it.
                conf.put(key, temp);
                break;
            }
            case 4: // Wait for ',' separator
            {
                // Do nothing, thats ok
                break;
            }
            default: //Out of known states, must not occurs
            {
                break;
            }
        }

        return new DiscoveredDevice(driver, conf);
    }

    /**
     * Generate an array of String representing command arguments to pass to
     * nut-scanner, when launching it.
     * @return Array of String of command arguments.
     */
    String[] generateCommandParameters() {
        ArrayList/*<String>*/ list = new ArrayList/*<String>*/();

        // Command name
        String localExecName = getExecName();
        if (localExecName != null && !localExecName.isEmpty()) {
            list.add(localExecName);
        } else {
            list.add(NUT_SCANNNER_EXEC_NAME);
        }
        list.add(PARAM_PARSABLE);
        list.add(PARAM_QUIET);

        // Scan Type
        int type = getScanType();
        if (type == SCAN_COMPLETE) {
            list.add(PARAM_SCAN_COMPLETE);
        } else {
            if ((type & SCAN_USB) != 0) {
                list.add(PARAM_SCAN_USB);
            }
            if ((type & SCAN_SNMP) != 0) {
                list.add(PARAM_SCAN_SNMP);
            }
            if ((type & SCAN_XML) != 0) {
                list.add(PARAM_SCAN_XML);
            }
            if ((type & SCAN_OLDNUT) != 0) {
                list.add(PARAM_SCAN_OLDNUT);
            }
            if ((type & SCAN_AVAHI) != 0) {
                list.add(PARAM_SCAN_AVAHI);
            }
            if ((type & SCAN_IPMI) != 0) {
                list.add(PARAM_SCAN_IPMI);
            }
        }

        // Options
        Map/*<String,String>*/ conf = getConfig();
        Set/*<String,String>*/ set = conf != null ? conf.entrySet() : null;
        if (set != null) {
            Iterator iter = set.iterator();
            while (iter.hasNext()) {
                Map.Entry/*<String,String>*/ entry = (Map.Entry) iter.next();
                if (entry != null) {
                    String name = (String) entry.getKey();
                    String value = (String) entry.getValue();
                    if (name != null && !name.isEmpty()) {
                        for (int i = 0; i < PARAM_COUPLE_DEFINITION.length; i += 2) {
                            if (name.equals(PARAM_COUPLE_DEFINITION[i])) {
                                list.add(PARAM_COUPLE_DEFINITION[i + 1]);
                                list.add(value);
                                break;
                            }
                        }
                    }
                }
            }
        }

        return (String[]) list.toArray(new String[list.size()]);
    }
}
