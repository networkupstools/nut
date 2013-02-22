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
#include <vector>

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

	void setName(const std::string& name);
	void setDriver(const std::string& driver);
	void setPort(const std::string& port);


	bool hasProperty(const std::string& name)const;
	std::string getProperty(const std::string& name)const;
	void setProperty(const std::string& name, const std::string& value);

	typedef std::map<std::string, std::string> PropertyMap;
	const PropertyMap& getProperties()const;
	typedef PropertyMap::const_iterator property_iterator;
	property_iterator property_begin()const;
	property_iterator property_end()const;

	typedef unsigned int Status;
	enum DeviceStatus {
		DEVICE_MONITORED	 =  1
	};

	Status getStatus()const;
	void setStatus(Status status);
	bool hasStatus(Status status)const;
	void setSubStatus(Status status, bool state = true);

	bool isMonitored()const{return hasStatus(DEVICE_MONITORED);}


protected:
	Device(const std::string& name = "");
	Device(nutscan_device_t* dev, const std::string& name = "");
	Device(const nut::GenericConfigSection& section);

	void initFromScanner(nutscan_device_t* dev);
	void initFromGenericSection(const nut::GenericConfigSection& section);

	std::string _name;
	PropertyMap _properties;
	Status _status;
};





/**
 * Global control module.
 */
class Controller : public std::list<Device*>
{
public:
	/**
	 * Default constructor.
	 */
	Controller();

	/**
	 * Constructor specifying parameters.
	 * \param ups_conf_path Default path of ups.conf file.
	 */
	Controller(const std::string& ups_conf_path);

	void init();

	/** Device manipulation
	 * \{ */

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
	 * Retrieve a list of devices from their names.
	 * \param Device name list.
	 * \return Device list. Device can be NULL if not found.
	 */
	std::vector<Device*> getDevices(const std::vector<std::string>& devices)const;

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

	/** \} */

	/** Device monitoring.
	 * \{ */

	/**
	 * Monitor some devices.
	 * \param devices List of device names to monitor.
	 */
	void monitorDevices(const std::vector<Device*>& devices);

	/**
	 * Monitor some devices.
	 * \param devices List of device names to monitor.
	 */
	void monitorDevices(const std::vector<std::string>& devices)
		{monitorDevices(getDevices(devices));}

	/**
	 * Unmonitor some devices.
	 * \param devices List of device names to unmonitor.
	 */
	void unmonitorDevices(const std::vector<Device*>& devices);

	/**
	 * Unmonitor some devices.
	 * \param devices List of device names to unmonitor.
	 */
	void unmonitorDevices(const std::vector<std::string>& devices)
		{unmonitorDevices(getDevices(devices));}

	/** \} */
	
	/** Driver managerment
   * \{ */
  /**
   * Start a driver instance from the device name.
   * \param name Driver device name.
   */
  void startDriver(const std::string& name);
  /**
   * Stop a driver instance from the device name.
   * \param name Driver device name.
   */
  void stopDriver(const std::string& name);
  /** \} */	 

	/** Device loading and scanning
	 * \{ */

	/**
	 * Load devices from ups.conf
	 * \param path ups.conf file path
	 * \return List of read devices.
	 */
	std::vector<std::string> loadUpsConf(const std::string& path);

	/**
	 * Load devices from ups.conf file at default path.
	 * \return List of read devices.
	 */
	std::vector<std::string> loadUpsConf();

	/**
	 * Scan USB devices.
	 * \return List of scanned devices.
	 */
	std::vector<std::string> scanUSB();

	/**
	 * Scan network devices with avahi.
	 * \return List of scanned devices.
	 */
	std::vector<std::string> scanAvahi(long usecTimeout);

	/**
	 * Scan network devices with XML-HTTP.
	 * \return List of scanned devices.
	 */
	std::vector<std::string> scanXMLHTTP(long usecTimeout);
	
	/**
	 * Scan network devices with nut classic.
	 * \return List of scanned devices.
	 */
	std::vector<std::string> scanNut(const std::string& startIP, const std::string& stopIP, unsigned short port, long usecTimeout);

	/**
	 * Scan network SNMPv1 devices.
	 * \return List of scanned devices.
	 */
	std::vector<std::string> scanSNMPv1(const std::string& startIP, const std::string& stopIP, long usecTimeout, const std::string& communityName);

	/**
	 * Scan network SNMPv3 devices.
	 * \return List of scanned devices.
	 */
	std::vector<std::string> scanSNMPv3(const std::string& startIP, const std::string& stopIP, long usecTimeout,
		const std::string& username, unsigned int securityLevel,
		const std::string& authMethod, const std::string& authPassword,
		const std::string& privMethod, const std::string& privPassword);

	/** \} */
protected:

	/**
	 * Flush _ups_conf to system ups.conf file.
	 */
	void flushUpsConfToFile();



	std::string _ups_conf_path;

	GenericConfiguration _ups_conf;

};



}} // Namespace nut::ctld

#endif // CONFIGURATION_HPP_SEEN
