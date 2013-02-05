/* control.cpp - Nut controller deamon - Controller

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

#include "control.hpp"

#include "nutstream.hpp"
#include "nutipc.hpp"

#include <sstream>
#include <iomanip>


extern "C" {

#include "common.h"

void rewind_list(nutscan_device_t** dev)
{
	if(*dev!=NULL)
	{
		while(true)
		{
			nutscan_device_t* prev = (*dev)->prev;
			if(prev==NULL)
				break;
			*dev = prev;
		}
	}
}

} // extern "C"

namespace nut {
namespace ctl {






//
// Device
//
Device::Device(const std::string& name):
_name(name),
_status(0)
{
}

Device::Device(nutscan_device_t* dev, const std::string& name):
_name(name),
_status(0)
{
	initFromScanner(dev);
}

Device::Device(const nut::GenericConfigSection& section):
_name(""),
_status(0)
{
	initFromGenericSection(section);
}

Device::Status Device::getStatus()const
{
	return _status;
}

void Device::setStatus(Device::Status status)
{
	_status = status;
}

bool Device::hasStatus(Device::Status status)const
{
	return (_status & status) == status;
}

void Device::setSubStatus(Device::Status status, bool state)
{
	if(state)
		_status |= status;
	else
		_status &= ~status;
}

std::string Device::getName()const
{
	return _name;
}

void Device::setName(const std::string& name)
{
	_name = name;
}

std::string Device::getDriver()const
{
	return getProperty("driver");
}

std::string Device::getPort()const
{
	return getProperty("port");
}

bool Device::hasProperty(const std::string& name)const
{
	return _properties.find(name) != _properties.end();
}

std::string Device::getProperty(const std::string& name)const
{
	return _properties.find(name)->second;
}

const Device::PropertyMap& Device::getProperties()const
{
	return _properties;
}

Device::property_iterator Device::property_begin()const
{
	return _properties.begin();
}

Device::property_iterator Device::property_end()const
{
	return _properties.end();
}


void Device::setDriver(const std::string& driver)
{
	_properties["driver"] = driver;
}

void Device::setPort(const std::string& port)
{
	_properties["port"] = port;
}

void Device::setProperty(const std::string& name, const std::string& value)
{
	_properties[name] = value;
}

void Device::initFromGenericSection(const nut::GenericConfigSection& section)
{
	_name = section.name;

	for(std::map<std::string, nut::GenericConfigSectionEntry>::const_iterator
		it = section.entries.begin(); it != section.entries.end(); ++it)
	{
		setProperty(it->first, it->second.values.size()>0 ? it->second.values.front() : "");
	}
}

void Device::initFromScanner(nutscan_device_t* dev)
{
	if(dev)
	{
		setDriver(dev->driver);
		setPort(dev->port);

		nutscan_options_t *opt = &dev->opt;
		while(opt)
		{
			if(opt->option!=NULL)
			{
				setProperty(opt->option, opt->value);
			}
			opt = opt->next;
		}
	}
}



//
// Controller
//

Controller::Controller()
{
	nutscan_init();
}

Controller::Controller(const std::string& ups_conf_path):
_ups_conf_path(ups_conf_path)
{
	nutscan_init();
}

bool Controller::addDevice(Device* device)
{
	// TODO Add data merging instead of adding.

	static unsigned int count = 0;

	std::string name = device->getName();

	if(name.empty() || getDevice(name))
	{
		do
		{
			std::ostringstream stm;
			stm << "dev" << std::setfill('0') << std::setw(4) << count++;
			stm.flush();
			name = stm.str();
		}while(getDevice(name));
		device->setName(name);
	}
	push_back(device);

	return true;
}

const Device* Controller::getDevice(const std::string& name)const
{
	for(const_iterator it=begin(); it!=end(); ++it)
	{
		if((*it)->getName()==name)
			return *it;
	}
	return NULL;
}

Device* Controller::getDevice(const std::string& name)
{
	for(iterator it=begin(); it!=end(); ++it)
	{
		if((*it)->getName()==name)
			return *it;
	}
	return NULL;
}

void Controller::removeDevice(const std::string& name)
{
	Device* dev = getDevice(name);
	if(dev)
	removeDevice(dev);
}


void Controller::removeDevice(Device* device)
{
	if(device)
	{
		iterator it = std::find(begin(), end(), device);
		if(it != end())
		{
			// TODO Test if device is (at-least) monitored and de-monitor it.

			// Delete device object
			delete device;
			// Remove it from list.
			erase(it);
		}
	}
}

std::vector<Device*> Controller::getDevices(const std::vector<std::string>& devices)const
{
	std::vector<Device*> res;
	res.reserve(devices.size());
	for(std::vector<std::string>::const_iterator it=devices.begin(); it!=devices.end(); ++it)
		res.push_back(const_cast<Device*>(getDevice(*it)));
	return res;
}

std::vector<std::string> Controller::loadUpsConf(const std::string& path)
{
	std::vector<std::string> res;

	NutFile file(path);
	file.open();
	_ups_conf.parseFrom(file);

	// Traverse ups.conf file to create devices.
	for(std::map<std::string, nut::GenericConfigSection>::iterator 
		it = _ups_conf.sections.begin();
		it != _ups_conf.sections.end(); ++it)
	{
		Device* device = new Device(it->second);
		addDevice(device);
		res.push_back(device->getName());
	}

	return res;
}

std::vector<std::string> Controller::loadUpsConf()
{
	if(!_ups_conf_path.empty())
		return loadUpsConf(_ups_conf_path);
	else
		return std::vector<std::string>();
}


std::vector<std::string> Controller::scanUSB()
{
	std::vector<std::string> res;

std::clog << "Controller::scanUSB()" << std::endl;

	nutscan_device_t *scan = nutscan_scan_usb(), *iter;
	rewind_list(&scan);
	iter = scan;
	while(iter)
	{
		Device* device = new Device(iter);
		addDevice(device);
		res.push_back(device->getName());
		iter = iter->next;
	}
	nutscan_free_device(scan);

	return res;
}

std::vector<std::string> Controller::scanAvahi(long usecTimeout)
{
	std::vector<std::string> res;

std::clog << "Controller::scanAvahi(" << usecTimeout << ")" << std::endl;

	nutscan_device_t *scan = nutscan_scan_avahi(usecTimeout), *iter;
	rewind_list(&scan);
	iter = scan;
	while(iter)
	{
		Device* device = new Device(iter);
		addDevice(device);
		res.push_back(device->getName());
		iter = iter->next;
	}
	nutscan_free_device(scan);

	return res;		
}

std::vector<std::string> Controller::scanXMLHTTP(long usecTimeout)
{
	std::vector<std::string> res;

std::clog << "Controller::scanXMLHTTP(" << usecTimeout << ")" << std::endl;

	nutscan_device_t *scan = nutscan_scan_xml_http(usecTimeout), *iter;
	rewind_list(&scan);
	iter = scan;
	while(iter)
	{
		Device* device = new Device(iter);
		addDevice(device);
		res.push_back(device->getName());
		iter = iter->next;
	}
	nutscan_free_device(scan);

	return res;		
}


std::vector<std::string> Controller::scanNut(const std::string& startIP, const std::string& stopIP, unsigned short port, long usecTimeout)
{
	std::vector<std::string> res;

	std::ostringstream oss;
	oss << port;

std::clog << "Controller::scanNut(" << startIP << ", " << stopIP << ", " << port << ", " << usecTimeout << ")" << std::endl;

	nutscan_device_t *scan = nutscan_scan_nut(startIP.c_str(), stopIP.c_str(), oss.str().c_str(), usecTimeout), *iter;
	rewind_list(&scan);
	iter = scan;
	while(iter)
	{
		Device* device = new Device(iter);
		addDevice(device);
		res.push_back(device->getName());
		iter = iter->next;
	}
	nutscan_free_device(scan);

	return res;
}

std::vector<std::string> Controller::scanSNMPv1(const std::string& startIP, const std::string& stopIP, long usecTimeout, const std::string& communityName)
{
	std::vector<std::string> res;

std::clog << "Controller::scanSNMPv1(" << startIP << ", " << stopIP << ", " << usecTimeout << ", " << communityName << ")" << std::endl;

	nutscan_snmp snmp;
	memset(&snmp, 0, sizeof(nutscan_snmp));
	snmp.community = strdup(communityName.c_str());

	nutscan_device_t *scan = nutscan_scan_snmp(startIP.c_str(), stopIP.c_str(), usecTimeout, &snmp), *iter;
	rewind_list(&scan);
	iter = scan;
	while(iter)
	{
		Device* device = new Device(iter);
		addDevice(device);
		res.push_back(device->getName());
		iter = iter->next;
	}
	nutscan_free_device(scan);

	return res;
}

std::vector<std::string> Controller::scanSNMPv3(const std::string& startIP, const std::string& stopIP, long usecTimeout,
		const std::string& username, unsigned int securityLevel,
		const std::string& authMethod, const std::string& authPassword, 
		const std::string& privMethod, const std::string& privPassword)
{
	static const char* secLevelNames[] = {"noAuthNoPriv", "authNoPriv", "authPriv"};
	std::vector<std::string> res;

std::clog << "Controller::scanSNMPv3(" << startIP << ", " << stopIP << ", " << usecTimeout
										 << ", " << username  << ", " << securityLevel
										 << ", " << authMethod  << ", " << authPassword
										 << ", " << privMethod  << ", " << privPassword
									     << ")" << std::endl;

std::clog << "Controller::scanSNMPv3() ignored as a bug is pending." << std::endl;

	if(securityLevel>2)
		return res;

	nutscan_snmp snmp;
	memset(&snmp, 0, sizeof(nutscan_snmp));
	snmp.secName      = strdup(username.c_str());
	snmp.secLevel     = strdup(secLevelNames[securityLevel]);
	if(securityLevel>0)
	{
		snmp.authProtocol = strdup(authMethod.c_str());
		snmp.authPassword = strdup(authPassword.c_str());
	}
	if(securityLevel==2)
	{
		snmp.privProtocol = strdup(privMethod.c_str());
		snmp.privPassword = strdup(privPassword.c_str());
	}
	nutscan_device_t *scan = nutscan_scan_snmp(startIP.c_str(), stopIP.c_str(), usecTimeout, &snmp), *iter;
	rewind_list(&scan);
	iter = scan;
	while(iter)
	{
		Device* device = new Device(iter);
		addDevice(device);
		res.push_back(device->getName());
		iter = iter->next;
	}
	nutscan_free_device(scan);

	return res;
}

void Controller::monitorDevices(const std::vector<Device*>& devices)
{
	for(std::vector<Device*>::const_iterator it=devices.begin(); it!=devices.end(); ++it)
	{
		Device* dev = const_cast<Device*>(*it);
		if(dev)
		{
			if(!dev->isMonitored())
			{
				// Add ups.conf section for device
				GenericConfigSection& section = _ups_conf.sections[dev->getName()];
				section.name = dev->getName();
				for(Device::property_iterator prit=dev->property_begin(); prit!=dev->property_end(); ++prit)
				{
					GenericConfigSectionEntry& entry = section.entries[prit->first];
					entry.name = prit->first;
					entry.values.push_back(prit->second);
				}

				// Change status to "monitored"
				dev->setSubStatus(Device::DEVICE_MONITORED);
			}
		}
	}

	flushUpsConfToFile();

	// Run drivers in consequences
	for(std::vector<Device*>::const_iterator it=devices.begin(); it!=devices.end(); ++it)
	{
		const Device* dev = *it;
		if(dev)
		{
			std::string name = dev->getName();
			std::list<std::string> params;
			params.push_back("start");
			params.push_back(name);
			try
			{
// TODO Wait that Vasek finish to implement Executor or try other way.
				nut::Process::Execution exec(DRVPATH "/upsdrvctl", params);
				exec.wait();

			}
			catch(std::runtime_error& ex)
			{
				std::cerr << "Runtime error while intending to execute upsdrvctl:" << std::endl
					 << ex.what() << std::endl;
			}
		}
	}	

	// Signal upsd that ups.conf has been changed
	sendsignal("upsd", SIGHUP);
}

void Controller::unmonitorDevices(const std::vector<Device*>& devices)
{
	// TODO
}

void Controller::flushUpsConfToFile()
{
	if(!_ups_conf_path.empty())
	{
		// Flush _ups_conf to ups.conf file
		NutFile file(_ups_conf_path);
		file.open(NutFile::WRITE_ONLY);
		_ups_conf.writeTo(file);
	}
}


}} // Namespace nut::ctld

