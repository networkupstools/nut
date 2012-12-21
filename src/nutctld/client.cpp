/* client.cpp - Nut controller deamon

   Copyright (C)
	2012	Emilien Kia <emilien.kia@gmail.com>

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

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <locale>
#include <sstream>
using namespace std;

#include "libnutctl.hpp"
using namespace org::networkupstools;


extern "C"
{
/* From nut scanner */
int nutscan_cidr_to_ip(const char * cidr, char ** start_ip, char ** stop_ip);
}

NutController* ctl = NULL;


int timeout = 5, secLevel = -1;
unsigned short port = 3493;
std::string startIp, endIp;
std::string community, secName, authProtocol, authPassword, privProtocol, privPassword;




template<typename type>
void displayPerLine(const type& str) {
  cout << str << endl;
}

void listDevices()
{
	std::vector< std::string > names = ctl->GetDeviceNames();
	for_each(names.begin(), names.end(), displayPerLine<string>);
}

void displayDeviceDetails(const std::string& name)
{
	std::map< std::string, std::string > params = ctl->GetDevice(name);

	if(params.empty())
		cerr << "No device named '" << name << "' found." << endl;
	else
	{
		for(std::map< std::string, std::string >::iterator it = params.begin();
			it != params.end(); ++it)
		{
			cout << it->first << " = " << it->second << endl;
		}
	}
}

void displayDeviceVariable(const std::string& device, const std::string& variable)
{
	cout <<  ctl->GetDeviceVariable(device, variable) << endl;
}

void setDeviceVariable(const std::string& device, const std::string& variable, const std::string& value)
{
	ctl->SetDeviceVariable(device, variable, value);
}

void monitorDevice(const std::string& device)
{
	ctl->MonitorDevice(device);
}

void unmonitorDevice(const std::string& device)
{
	ctl->UnmonitorDevice(device);
}

void scanUSB()
{
	std::vector< std::string > names = ctl->ScanUSB();
	for_each(names.begin(), names.end(), displayPerLine<string>);
}

void scanXML()
{
	std::vector< std::string > names = ctl->ScanXMLHTTP(timeout * 1000000);
	for_each(names.begin(), names.end(), displayPerLine<string>);
}

void scanAvahi()
{
	std::vector< std::string > names = ctl->ScanAvahi(timeout * 1000000);
	for_each(names.begin(), names.end(), displayPerLine<string>);
}

void scanOld()
{
	std::vector< std::string > names = ctl->ScanNut(startIp, endIp, port, timeout * 1000000);
	for_each(names.begin(), names.end(), displayPerLine<string>);
}

void scanSNMP()
{
	std::vector< std::string > names;

	if(!community.empty())	// SNMPv1
	{
		names = ctl->ScanSNMPv1(startIp, endIp, timeout * 1000000, community);
	}
	else if(!secName.empty()) // SNMPv3
	{
		// If not provided, detect security level
		if(secLevel==-1)
		{
			if(!privPassword.empty())
				secLevel = 2;
			else if(!authPassword.empty())
				secLevel = 1;
			else
				secLevel = 0;
		}
		// Set default protocols if needed
		if(authProtocol.empty())
			authProtocol = "MD5";
		if(privProtocol.empty())
			privProtocol = "DES";
		// DO !
		names = ctl->ScanSNMPv3(startIp, endIp, timeout * 1000000, secName, secLevel, authProtocol, authPassword, privProtocol, privPassword);
	}

	for_each(names.begin(), names.end(), displayPerLine<string>);
}


void displayHelp()
{
	cout << "nutctl client" << endl
		<< "Usage: nutctl [OPTIONS] COMMANDS" << endl
		<< endl
		<< "COMMANDS:" << endl
		<< "  -l, --list                 List device names" << endl
		<< "  -d, --details <devname>    Display details of device 'devname'" << endl
		<< "  --get <devname> <varname>  Display value of a variable for the specified device" << endl
		<< "  --set <devname> <varname> <value>    Set value for the specified variable of a device" << endl
		<< "  --monitor <devname>        Monitor a device" << endl
		<< "  --unmonitor <devname>      Unmonitor a device" << endl
		<< endl
		<< "Scan commands:" << endl
		<< "  -C, --scan-complete        Scan all available devices" << endl
		<< "  -U, --scan-usb             Scan USB devices" << endl
		<< "  -S, --scan-snmp            Scan SNMP devices" << endl
		<< "  -M, --scan-xml             Scan XML devices" << endl
		<< "  -A, --scan-avahi           Scan NUT devices (avahi method)" << endl
		<< "  -O, --scan-oldnut          Scan NUT devices (old method)" << endl

		<< endl
		<< "OPTIONS:" << endl
		<< "  -t, --timeout <sec>        Set the timeout in seconds" << endl
		<< "  -s, --start-ip <ip>        First IP address to scan" << endl
		<< "  -e, --end-ip <ip>          Last IP address to scan" << endl
		<< "  -m, --mask <IP/mask>       Give a range of IP using CIDR notation" << endl
		
		<< endl
		<< "SNMP v1 specific options:" << endl
		<< "  -c, --community <community name> Set SNMP v1 community name (default = public)" << endl
		<< endl
		<< "SNMP v3 specific options:" << endl
		<< "  -v, --secLevel <security level>  Set the securityLevel used for SNMPv3 messages (allowed values: noAuthNoPriv,authNoPriv,authPriv)" << endl
		<< "  -u, --secName <security name>    Set the securityName used for authenticated SNMPv3 messages (mandatory if you set secLevel. No default)" << endl
		<< "  -w, --authProtocol <authentication protocol>    Set the authentication protocol (MD5 or SHA) used for authenticated SNMPv3 messages (default=MD5)" << endl
		<< "  -W, --authPassword <authentication pass phrase> Set the authentication pass phrase used for authenticated SNMPv3 messages (mandatory if you set secLevel to authNoPriv or authPriv)" << endl
		<< "  -x, --privProtocol <privacy protocol>           Set the privacy protocol (DES or AES) used for encrypted SNMPv3 messages (default=DES)" << endl
		<< "  -X, --privPassword <privacy pass phrase>        Set the privacy pass phrase used for encrypted SNMPv3 messages (mandatory if you set secLevel to authPriv)" << endl
		<< endl
		<< "NUT specific options:" << endl
		<< "  -p, --port <port number>: Port number of remote NUT upsd" << endl
		;
}


int main(int argc, char** argv)
{
	ctl = NutController::getController();

	enum ACTIONS {
		UNKNOWN,
		DEVICE_DETAILS,
		GET,
		SET,
		MONITOR,
		UNMONITOR,
		SCAN_COMPLETE,
		SCAN_USB,
		SCAN_SNMP,
		SCAN_XML,
		SCAN_AVAHI,
		SCAN_OLD
	}action = UNKNOWN;

	std::vector<std::string> params;

	for(int i=1; i<argc; ++i)
	{
		std::string str = argv[i];

		if(str.size()==0)
			continue;

		if(str=="--list" || str=="-l")
		{
			listDevices();
			return 0;
		}
		else if(str=="--details" || str=="-d")
		{
			action = DEVICE_DETAILS;
		}
		else if(str=="--get")
		{
			action = GET;
		}
		else if(str=="--set")
		{
			action = SET;
		}
		else if(str=="--monitor")
		{
			action = MONITOR;
		}
		else if(str=="--unmonitor")
		{
			action = UNMONITOR;
		}
		else if(str=="-C" || str=="--scan-complete" || str=="--usb_scan")
		{
			action = SCAN_COMPLETE;
		}
		else if(str=="-U" || str=="--scan-usb" || str=="--usb_scan")
		{
			action = SCAN_USB;
		}
		else if(str=="-S" || str=="--scan-snmp" || str=="--snmp_scan")
		{
			action = SCAN_SNMP;
		}
		else if(str=="-M" || str=="--scan-xml" || str=="--xml_scan")
		{
			action = SCAN_XML;
		}
		else if(str=="-A" || str=="--scan-avahi" || str=="--avahi_scan")
		{
			action = SCAN_AVAHI;
		}
		else if(str=="-O" || str=="--scan-oldnut" || str=="--oldnut_scan")
		{
			action = SCAN_OLD;
		}
		else if(str=="-t" || str=="--timeout")
		{
			if(++i<argc)
			{
				istringstream stm(string(argv[i]));
				stm >> timeout;
			}
		}
		else if(str=="-p" || str=="--port")
		{
			if(++i<argc)
			{
				istringstream stm(string(argv[i]));
				stm >> port;
			}
		}
		else if(str=="-s" || str=="--start-ip" || str=="--start_ip")
		{
			if(++i<argc)
				startIp = argv[i];
		}
		else if(str=="-e" || str=="--end-ip" || str=="--end_ip")
		{
			if(++i<argc)
				endIp = argv[i];
		}
		else if(str=="-m" || str=="--mask" || str=="--mask-cidr" || str=="--mask_cidr")
		{
			if(++i<argc)
			{
				std::string mask = argv[i];
				char *start, *stop;
				if(nutscan_cidr_to_ip(mask.c_str(), &start, &stop)==1)
				{
					startIp = start;
					endIp = stop;
					free(start);
					free(stop);
				}
			}
		}
		else if(str=="-c" || str=="--community")
		{
			if(++i<argc)
				community = argv[i];
		}
		else if(str=="-v" || str=="--secLevel" || str=="--securityLevel"
							|| str=="--sec-level" || str=="--security-level"
							|| str=="--sec_level" || str=="--security_level")
		{
			if(++i<argc)
			{
				std::string str = argv[i];
				if(str=="authPriv" || str=="2")
					secLevel = 2;
				else if(str=="authNoPriv" || str=="1")
					secLevel = 1;
				else if(str=="noAuthNoPriv" || str=="0")	
					secLevel = 0;
				else
				{
					cerr << "Security level '" << str << "' not recognized." << endl;
				}
			}
		}
		else if(str=="-u" || str=="--secName" || str=="--securityName"
							|| str=="--sec-name" || str=="--security-name"
							|| str=="--sec_name" || str=="--security_name")
		{
			if(++i<argc)
				secName = argv[i];
		}
		else if(str=="-w" || str=="--authProtocol" || str=="--auth-protocol" || str=="--auth_protocol")
		{
			if(++i<argc)
				authProtocol = argv[i];
		}
		else if(str=="-W" || str=="--authPassword" || str=="--auth-password" || str=="--auth_password")
		{
			if(++i<argc)
				authPassword = argv[i];
		}
		else if(str=="-x" || str=="--privProtocol" || str=="--priv-protocol" || str=="--priv_protocol")
		{
			if(++i<argc)
				privProtocol = argv[i];
		}
		else if(str=="-X" || str=="--privPassword" || str=="--priv-password" || str=="--priv_password")
		{
			if(++i<argc)
				privPassword = argv[i];
		}
		else
		{
			params.push_back(str);
		}
	}

	switch(action)
	{
	case DEVICE_DETAILS:
		if(params.size() == 1)
			displayDeviceDetails(params[0]);
		else
			displayHelp();
		break;
	case GET:
		if(params.size() == 2)
			displayDeviceVariable(params[0], params[1]);
		else
			displayHelp();
		break;
	case SET:
		if(params.size() == 2)
			setDeviceVariable(params[0], params[1], "");
		else if(params.size() == 3)
			setDeviceVariable(params[0], params[1], params[2]);
		else
			displayHelp();
		break;
	case MONITOR:
		if(params.size() == 1)
			monitorDevice(params[0]);
		else
			displayHelp();
		break;
	case UNMONITOR:
		if(params.size() == 1)
			unmonitorDevice(params[0]);
		else
			displayHelp();
		break;
	case SCAN_USB:
		scanUSB();
		break;
	case SCAN_XML:
		scanXML();
		break;
	case SCAN_AVAHI:
		scanAvahi();
		break;
	case SCAN_OLD:
		scanOld();
		break;
	case SCAN_SNMP:
		scanSNMP();
		break;
	default:
		displayHelp();
		break;
	}

	return 0;
}
