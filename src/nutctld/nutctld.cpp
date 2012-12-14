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


#include <iostream>
using namespace std;

#include "control.hpp"
using namespace nut::ctl;

#include "nutctl_adaptor.hpp"


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
	virtual void RemoveDevice(const std::string& name);
    virtual std::vector< std::string > ScanUSB();
};


std::vector<std::string> DBusNutCtl::GetDeviceNames()
{
	std::vector<std::string> res;

	for(Controller::const_iterator it=Controller::get().begin();
		it!=Controller::get().end(); ++it)
	{
		res.push_back((*it)->getName());
	}

	return res;
}


std::map< std::string, std::string > DBusNutCtl::GetDevice(const std::string& name)
{
	std::map< std::string, std::string > res;

	const Device* dev = Controller::get().getDevice(name);
	if(dev)
	{
		res = dev->getOptions();
/*		for(Node::option_iterator it=node->option_begin(); it!=node->option_end(); ++it)
		{
			res[it->first] = it->second;
		}*/
	}

	return res;
}

void DBusNutCtl::RemoveDevice(const std::string& name)
{
	Controller::get().removeDevice(name);
}

std::vector< std::string > DBusNutCtl::ScanUSB()
{
	std::list<std::string> scan = Controller::get().scanUSB();
	return std::vector< std::string >(scan.begin(), scan.end());
}


DBus::BusDispatcher dispatcher;


int main(int argc, char** argv)
{
	cout << "nutctld" << endl;

	Controller::get().load();


	DBus::default_dispatcher = &dispatcher;
    DBus::Connection bus = DBus::Connection::SessionBus();
//    DBus::Connection bus = DBus::Connection::SystemBus();

	bus.request_name(DBUS_NUTCTL_NAME);

	DBusNutCtl ctl(bus);

	dispatcher.enter();

	return 0;
}
