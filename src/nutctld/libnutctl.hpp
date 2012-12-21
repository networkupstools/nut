/* libnutctl.hpp - Nut controller client interface

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

#include <map>
#include <string>
#include <vector>

namespace org {
namespace networkupstools {


/**
 * Interface for sending commands and retrieving data to/from Nut controller system instance.
 */
class NutController
{
public:

	virtual std::vector< std::string > GetDeviceNames() =0;

	virtual std::map< std::string, std::string > GetDevice(const std::string& name) =0;

	virtual std::string GetDeviceVariable(const std::string& device, const std::string& variable) =0;

	virtual void SetDeviceVariable(const std::string& device, const std::string& variable, const std::string& value) =0;

	virtual void MonitorDevice(const std::string& device) =0;

	virtual void UnmonitorDevice(const std::string& device) =0;

	virtual std::vector< std::string > ScanUSB() =0;

	virtual std::vector< std::string > ScanAvahi(long usecTimeout) =0;

	virtual std::vector< std::string > ScanXMLHTTP(long usecTimeout) =0;

	virtual std::vector< std::string > ScanNut(const std::string& startIP, const std::string& stopIP, unsigned short port, long usecTimeout) =0;

	virtual std::vector< std::string > ScanSNMPv1(const std::string& startIP, const std::string& stopIP, long usecTimeout, const std::string& communityName) =0;

	virtual std::vector< std::string > ScanSNMPv3(const std::string& startIP, const std::string& stopIP, long usecTimeout, const std::string& userName, int securityLevel, const std::string& authMethod, const std::string& authPassword, const std::string& privMethod, const std::string& privPassword) =0;


	static NutController* getController();

};


}} // namespace org::networkupstools

