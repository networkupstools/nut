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

#include "config.h"

#include "nutstream.h"


extern "C" {
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

std::string Device::getDriver()const
{
	return getOption("driver");
}

std::string Device::getPort()const
{
	return getOption("port");
}

bool Device::hasOption(const std::string& optName)const
{
	return _options.find(optName) != _options.end();
}

std::string Device::getOption(const std::string& optName)const
{
	return _options.find(optName)->second;
}

const std::map<std::string, std::string>& Device::getOptions()const
{
	return _options;
}

Device::option_iterator Device::option_begin()const
{
	return _options.begin();
}

Device::option_iterator Device::option_end()const
{
	return _options.end();
}


void Device::setDriver(const std::string& driver)
{
	_options["driver"] = driver;
}

void Device::setPort(const std::string& port)
{
	_options["port"] = port;
}

void Device::setOption(const std::string& name, const std::string& value)
{
	_options[name] = value;
}

void Device::initFromGenericSection(const nut::GenericConfigSection& section)
{
	_name = section.name;

	for(std::map<std::string, nut::GenericConfigSectionEntry>::const_iterator
		it = section.entries.begin(); it != section.entries.end(); ++it)
	{
		setOption(it->first, it->second.values.size()>0 ? it->second.values.front() : "");
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
				setOption(opt->option, opt->value);
			}
			opt = opt->next;
		}
	}
}



//
// Controller
//

Controller Controller::_instance;

Controller& Controller::get()
{
	return _instance;
}

Controller::Controller()
{
}

void Controller::load()
{
	loadUpsConf();
}


static std::string _ups_conf_src = 
	"[ups1]\n"
	"driver = dummy-ups\n"
	"port = /here/you/are\n"
	"desc = \"this is a first dummy UPS\"\n"
	"\n"
	"[ups2]\n"
	"driver = dummy-ups\n"
	"port = /use/the/force/Luke\n"
	"desc = \"this is another dummy UPS\"\n"
;

void Controller::loadUpsConf()
{
	// Load ups.conf file
	// TODO Test from a string, change it with loading from real conf file
//	_ups_conf.parseFromString(_ups_conf_src);

std::cout << "Controller::loadUpsConf() : parse file '" << CONFPATH "/ups.conf" << "'" << std::endl;

	NutFile file(CONFPATH "/ups.conf");
	file.open();
	_ups_conf.parseFrom(file);

	// Traverse ups.conf file to create devices.
	for(std::map<std::string, nut::GenericConfigSection>::iterator 
		it = _ups_conf.sections.begin();
		it != _ups_conf.sections.end(); ++it)
	{
		addDevice(new Device(it->second));
	}
}


std::list<std::string> Controller::scanUSB()
{
	std::list<std::string> res;

	nutscan_device_t *scan = nutscan_scan_usb(), *iter;
	rewind_list(&scan);
	iter = scan;
	while(iter)
	{
		Device* device = new Device(iter);
		// TODO add a generated name if needed
		res.push_back(device->getName());
		addDevice(device);

		iter = iter->next;
	}
	nutscan_free_device(scan);

	return res;
}

bool Controller::addDevice(Device* device)
{
	// TODO Add data merging instead of adding.
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
}

Device* Controller::getDevice(const std::string& name)
{
	for(iterator it=begin(); it!=end(); ++it)
	{
		if((*it)->getName()==name)
			return *it;
	}
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



}} // Namespace nut::ctld

