/* libnutctl.hpp - Nut controller client implementation

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

#include "libnutctl.hpp"

#include "nutctl_proxy.hpp"

#define DBUS_NUTCTL_PATH "/org/networkupstools/NutCtl"
#define DBUS_NUTCTL_NAME "org.networkupstools.NutCtl"

namespace org {
namespace networkupstools {

//
// DBusNutController
//

class DBusNutController : public NutController
{
public:

	DBusNutController();
	virtual ~DBusNutController();

	virtual std::vector< std::string > GetDeviceNames();
	virtual std::map< std::string, std::string > GetDevice(const std::string& name);
	virtual std::string GetDeviceVariable(const std::string& device, const std::string& variable);
	virtual void SetDeviceVariable(const std::string& device, const std::string& variable, const std::string& value);
	virtual std::vector< std::string > ScanUSB();
	virtual std::vector< std::string > ScanAvahi(long usecTimeout);
	virtual std::vector< std::string > ScanXMLHTTP(long usecTimeout);
	virtual std::vector< std::string > ScanNut(const std::string& startIP, const std::string& stopIP, unsigned short port, long usecTimeout);
	virtual std::vector< std::string > ScanSNMPv1(const std::string& startIP, const std::string& stopIP, long usecTimeout, const std::string& communityName);
	virtual std::vector< std::string > ScanSNMPv3(const std::string& startIP, const std::string& stopIP, long usecTimeout, const std::string& userName, int securityLevel, const std::string& authMethod, const std::string& authPassword, const std::string& privMethod, const std::string& privPassword);


protected:

	
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

	DBusNutCtl* _dbus_ctl;
	DBus::Connection* _dbus_cnx;

	static DBus::BusDispatcher dispatcher;
};

DBus::BusDispatcher DBusNutController::dispatcher;


DBusNutController::DBusNutController()
{
	DBus::default_dispatcher = &dispatcher;

	_dbus_cnx = new DBus::Connection(DBus::Connection::SessionBus());
//	_dbus_cnx = DBus::Connection::SystemBus();

	_dbus_ctl = new DBusNutCtl(*_dbus_cnx);
}

DBusNutController::~DBusNutController()
{
	delete _dbus_ctl;
	_dbus_ctl = NULL;

	delete _dbus_cnx;
	_dbus_cnx = NULL;
}

std::vector< std::string > DBusNutController::GetDeviceNames()
{
	return _dbus_ctl->GetDeviceNames();
}

std::map< std::string, std::string > DBusNutController::GetDevice(const std::string& name)
{
	return _dbus_ctl->GetDevice(name);
}

std::string DBusNutController::GetDeviceVariable(const std::string& device, const std::string& variable)
{
	return _dbus_ctl->GetDeviceVariable(device, variable);
}

void DBusNutController::SetDeviceVariable(const std::string& device, const std::string& variable, const std::string& value)
{
	_dbus_ctl->SetDeviceVariable(device, variable, value);
}

std::vector< std::string > DBusNutController::ScanUSB()
{
	return _dbus_ctl->ScanUSB();
}

std::vector< std::string > DBusNutController::ScanAvahi(long usecTimeout)
{
	return _dbus_ctl->ScanAvahi(usecTimeout);
}

std::vector< std::string > DBusNutController::ScanXMLHTTP(long usecTimeout)
{
	return _dbus_ctl->ScanXMLHTTP(usecTimeout);

}

std::vector< std::string > DBusNutController::ScanNut(const std::string& startIP, const std::string& stopIP, unsigned short port, long usecTimeout)
{
	return _dbus_ctl->ScanNut(startIP, stopIP, port, usecTimeout);

}

std::vector< std::string > DBusNutController::ScanSNMPv1(const std::string& startIP, const std::string& stopIP, long usecTimeout, const std::string& communityName) 
{
	return _dbus_ctl->ScanSNMPv1(startIP, stopIP, usecTimeout, communityName);
}

std::vector< std::string > DBusNutController::ScanSNMPv3(const std::string& startIP, const std::string& stopIP, long usecTimeout, const std::string& userName, int securityLevel, const std::string& authMethod, const std::string& authPassword, const std::string& privMethod, const std::string& privPassword)
{
	return _dbus_ctl->ScanSNMPv3(startIP, stopIP, usecTimeout, userName, securityLevel, authMethod, authPassword, privMethod, privPassword);
}






//
// NutController
//

NutController* NutController::getController()
{
	return new DBusNutController;
}




}} // namespace org::networkupstools

