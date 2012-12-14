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


#include <iostream>
using namespace std;

//#include "configuration.hpp"
//using namespace nut::ctl;

#include "nutctl_proxy.hpp"

#define DBUS_NUTCTL_PATH "/org/networkupstools/NutCtl"
#define DBUS_NUTCTL_NAME "org.networkupstools.NutCtl"

class DBusNutCtl : public org::networkupstools::NutCtl_proxy,
				public DBus::IntrospectableProxy,
				public DBus::ObjectProxy
{
public:
	DBusNutCtl(DBus::Connection &connection):
	DBus::ObjectProxy(connection, DBUS_NUTCTL_PATH, DBUS_NUTCTL_NAME)
	{
	}
};


DBus::BusDispatcher dispatcher;


int main(int argc, char** argv)
{
	cout << "nutctl client" << endl;

	DBus::default_dispatcher = &dispatcher;
    DBus::Connection bus = DBus::Connection::SessionBus();
//    DBus::Connection bus = DBus::Connection::SystemBus();

	DBusNutCtl ctl(bus);

	std::vector< std::string > res = ctl.GetDeviceNames();
	for(size_t n=0; n<res.size(); n++)
	{
		cout << " - " << res[n] << endl;
		std::map< std::string, std::string > vars = ctl.GetDevice(res[n]);
		for(std::map< std::string, std::string >::iterator it=vars.begin(); it!=vars.end(); ++it)
		{
			cout << "    - " << it->first << " : " << it->second << std::endl;
		}
	}

	return 0;
}
