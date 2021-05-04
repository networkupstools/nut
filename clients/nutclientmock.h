/* nutclientmock.h - definitions for nutclientmock C/C++ library

   Copyright (C) 2021

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

#ifndef NUTCLIENTMOCK_HPP_SEEN
#define NUTCLIENTMOCK_HPP_SEEN

/* Begin of C++ nutclient library declaration */
#ifdef __cplusplus

#include "nutclient.h"

namespace nut
{

/**
 * TCP NUTD client mock.
 * Class to mock TCPClient for test.
 */
class TcpClientMock : public Client
{
public:
	/**
	 * Construct a nut TcpClientMock object.
	 */
	TcpClientMock() {};
	~TcpClientMock() {};

	virtual void authenticate(const std::string& user, const std::string& passwd) {};
	virtual void logout() {};

	virtual Device getDevice(const std::string& name);
	virtual std::set<std::string> getDeviceNames();
	virtual std::string getDeviceDescription(const std::string& name);

	virtual std::set<std::string> getDeviceVariableNames(const std::string& dev);
	virtual std::set<std::string> getDeviceRWVariableNames(const std::string& dev);
	virtual std::string getDeviceVariableDescription(const std::string& dev, const std::string& name);
	virtual std::vector<std::string> getDeviceVariableValue(const std::string& dev, const std::string& name);
	virtual std::map<std::string,std::vector<std::string> > getDeviceVariableValues(const std::string& dev);
	virtual std::map<std::string,std::map<std::string,std::vector<std::string> > > getDevicesVariableValues(const std::set<std::string>& devs);
	virtual TrackingID setDeviceVariable(const std::string& dev, const std::string& name, const std::string& value);
	virtual TrackingID setDeviceVariable(const std::string& dev, const std::string& name, const std::vector<std::string>& values);

	virtual std::set<std::string> getDeviceCommandNames(const std::string& dev);
	virtual std::string getDeviceCommandDescription(const std::string& dev, const std::string& name);
	virtual TrackingID executeDeviceCommand(const std::string& dev, const std::string& name, const std::string& param="");

 	virtual void deviceLogin(const std::string& dev);
	virtual void deviceMaster(const std::string& dev);
	virtual void deviceForcedShutdown(const std::string& dev);
	virtual int deviceGetNumLogins(const std::string& dev);

	virtual TrackingResult getTrackingResult(const TrackingID& id);

	virtual bool isFeatureEnabled(const Feature& feature);
	virtual void setFeature(const Feature& feature, bool status);

private:
	std::map<std::string, std::map<std::string, std::vector<std::string>>> _values;
};

} /* namespace nut */

#endif /* __cplusplus */
/* End of C++ nutclient library declaration */

#endif	/* NUTCLIENTMOCK_HPP_SEEN */
