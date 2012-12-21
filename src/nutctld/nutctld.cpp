/* nutctld.cpp - Nut controller deamon

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

#include "config.h"

#include <iostream>
using namespace std;

#include "control.hpp"
using namespace nut::ctl;

#include "nutctl_adaptor.hpp"


Controller controller(CONFPATH "/ups.conf");

// DRVPATH


#define DBUS_NUTCTL_PATH "/org/networkupstools/NutCtl"
#define DBUS_NUTCTL_NAME "org.networkupstools.NutCtl"

class DBusNutCtl : public org::networkupstools::NutCtl_adaptor,
				public DBus::IntrospectableAdaptor,
				public DBus::ObjectAdaptor
{
public:
	DBusNutCtl(DBus::Connection &connection):
		DBus::ObjectAdaptor(connection, DBUS_NUTCTL_PATH)
	{
	}

    virtual std::vector< std::string > GetDeviceNames();
    virtual std::map< std::string, std::string > GetDevice(const std::string& name);
    virtual std::string GetDeviceVariable(const std::string& device, const std::string& variable);
    virtual void SetDeviceVariable(const std::string& device, const std::string& variable, const std::string& value);
	virtual void RemoveDevice(const std::string& name);

    virtual void MonitorDevice(const std::string& name);
    virtual void UnmonitorDevice(const std::string& name);

    virtual std::vector< std::string > ScanUSB();
    virtual std::vector< std::string > ScanAvahi(const int32_t& usecTimeout);
    virtual std::vector< std::string > ScanXMLHTTP(const int32_t& usecTimeout);
    virtual std::vector< std::string > ScanNut(const std::string& startIP, const std::string& stopIP, const uint16_t& port, const int32_t& usecTimeout);
    virtual std::vector< std::string > ScanSNMPv1(const std::string& startIP, const std::string& stopIP, const int32_t& usecTimeout, const std::string& communityName);
	virtual std::vector< std::string > ScanSNMPv3(const std::string& startIP, const std::string& stopIP, const int32_t& usecTimeout, const std::string& userName, const int32_t& securityLevel, const std::string& authMethod, const std::string& authPassword, const std::string& privMethod, const std::string& privPassword);
};


std::vector<std::string> DBusNutCtl::GetDeviceNames()
{
	std::vector<std::string> res;

	for(Controller::const_iterator it=controller.begin(); it!=controller.end(); ++it)
	{
		res.push_back((*it)->getName());
	}

	return res;
}


std::map< std::string, std::string > DBusNutCtl::GetDevice(const std::string& name)
{
	std::map< std::string, std::string > res;

	const Device* dev = controller.getDevice(name);
	if(dev)
	{
		res = dev->getProperties();
		for(Device::property_iterator it=dev->property_begin(); it!=dev->property_end(); ++it)
		{
			res[it->first] = it->second;
		}
	}

	return res;
}

std::string DBusNutCtl::GetDeviceVariable(const std::string& device, const std::string& variable)
{
	const Device* dev = controller.getDevice(device);
	if(dev)
	{
		if(dev->hasProperty(variable))
			return dev->getProperty(variable);
	}
	else
	{
		return "";	
	}
}

void DBusNutCtl::SetDeviceVariable(const std::string& device, const std::string& variable, const std::string& value)
{
	Device* dev = controller.getDevice(device);
	if(dev)
	{
		dev->setProperty(variable, value);
	}
}

void DBusNutCtl::RemoveDevice(const std::string& name)
{
	controller.removeDevice(name);
}

void DBusNutCtl::MonitorDevice(const std::string& name)
{
	std::vector<std::string> names;
	names.push_back(name);
	controller.monitorDevices(names);
}

void DBusNutCtl::UnmonitorDevice(const std::string& name)
{
	std::vector<std::string> names;
	names.push_back(name);
	controller.unmonitorDevices(names);
}

std::vector< std::string > DBusNutCtl::ScanUSB()
{
	return controller.scanUSB();
}

std::vector< std::string > DBusNutCtl::ScanAvahi(const int32_t& usecTimeout)
{
	return controller.scanAvahi(usecTimeout);
}

std::vector< std::string > DBusNutCtl::ScanXMLHTTP(const int32_t& usecTimeout)
{
	return controller.scanXMLHTTP(usecTimeout);
}

std::vector< std::string > DBusNutCtl::ScanNut(const std::string& startIP, const std::string& stopIP, const uint16_t& port, const int32_t& usecTimeout)
{
	return controller.scanNut(startIP, stopIP, port, usecTimeout);
}

std::vector< std::string > DBusNutCtl::ScanSNMPv1(const std::string& startIP, const std::string& stopIP, const int32_t& usecTimeout, const std::string& communityName)
{
	return controller.scanSNMPv1(startIP, stopIP, usecTimeout, communityName);
}

std::vector< std::string > DBusNutCtl::ScanSNMPv3(const std::string& startIP, const std::string& stopIP, const int32_t& usecTimeout, const std::string& userName, const int32_t& securityLevel, const std::string& authMethod, const std::string& authPassword, const std::string& privMethod, const std::string& privPassword)
{
cerr << "SNMPv3 scanning is disabled due to a pending bug." << endl;
	return std::vector< std::string >();
//	return controller.scanSNMPv3(startIP, stopIP, usecTimeout, userName, securityLevel, authMethod, authPassword, privMethod, privPassword);
}


DBus::BusDispatcher dispatcher;


int main(int argc, char** argv)
{
	cout << "nutctld" << endl;

	controller.loadUpsConf();

	DBus::default_dispatcher = &dispatcher;
//    DBus::Connection bus = DBus::Connection::SessionBus();
    DBus::Connection bus = DBus::Connection::SystemBus();

	bus.request_name(DBUS_NUTCTL_NAME);

	DBusNutCtl ctl(bus);

	dispatcher.enter();

	return 0;
}
