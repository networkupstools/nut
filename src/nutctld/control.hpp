/* control.hpp - Nut controller deamon - Controller

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

#ifndef CONTROLLER_HPP_SEEN
#define CONTROLLER_HPP_SEEN


#include <algorithm>
#include <map>
#include <string>

#include "nutconf.h"


extern "C" {
#include <nut-scan.h>
} // extern "C"


namespace nut {
namespace ctl {

class Controller;
class Device;


/**
 * Device.
 */
class Device
{
	friend class Controller;
public:

	std::string getName()const;

	std::string getDriver()const;
	std::string getPort()const;

	bool hasOption(const std::string& optName)const;
	std::string getOption(const std::string& optName)const;

	void setDriver(const std::string& driver);
	void setPort(const std::string& port);
	void setOption(const std::string& name, const std::string& value);

	const std::map<std::string, std::string>& getOptions()const;
	typedef std::map<std::string, std::string>::const_iterator option_iterator;
	option_iterator option_begin()const;
	option_iterator option_end()const;

	typedef unsigned int Status;
	enum DeviceStatus {
		DEVICE_MONITORED	 =  1
	};

	Status getStatus()const;
	void setStatus(Status status);
	bool hasStatus(Status status)const;
	void setSubStatus(Status status, bool state);

	bool isMonitored()const{return hasStatus(DEVICE_MONITORED);}


protected:
	Device(const std::string& name = "");
	Device(nutscan_device_t* dev, const std::string& name = "");
	Device(const nut::GenericConfigSection& section);

	void initFromScanner(nutscan_device_t* dev);
	void initFromGenericSection(const nut::GenericConfigSection& section);

	std::string _name;
	std::map<std::string, std::string> _options;
	Status _status;
};





/**
 * Global control module.
 */
class Controller : public std::list<Device*>
{
protected:
	static Controller _instance;
	Controller();

	GenericConfiguration _ups_conf;

public:

	/**
     * Add device to the known device list.
	 * \return True if device was really added and
	 * false if the device is redundant with another device and destroyed.
	 */
	bool addDevice(Device* node);
	
	/**
	 * Retrieve a device description from its name.
	 * \param name Device name.
	 * \return Device description structure if found, NULL otherwise.
	 */
	const Device* getDevice(const std::string& name)const;

	/**
	 * Retrieve a device description from its name.
	 * \param name Device name.
	 * \return Device description structure if found, NULL otherwise.
	 */
	Device* getDevice(const std::string& name);

	/**
	 * Remove a device from the list.
	 * \param name Name of device to remove.
	 */
	void removeDevice(const std::string& name);

	/**
	 * Remove a device from the list.
	 * \param device Device to remove and destroy.
	 */
	void removeDevice(Device* device);




	void loadUpsConf();

	std::list<std::string> scanUSB();

	


	/**
	 * Retrieve the controller singleton instance.
	 */
	static Controller& get();

	void load();

};



}} // Namespace nut::ctld

#endif // CONFIGURATION_HPP_SEEN
