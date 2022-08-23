/* nutclientmem.cpp - nutclientmem C++ library implementation

   Copyright (C) 2021  Eric Clappier <ericclappier@eaton.com>

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
#include "nutclientmem.h"

namespace nut
{

/*
 *
 * Memory Client stub implementation
 *
 */

Device MemClientStub::getDevice(const std::string& name)
{
	NUT_UNUSED_VARIABLE(name);
	throw NutException("Not implemented");
}

std::set<std::string> MemClientStub::getDeviceNames()
{
	throw NutException("Not implemented");
}

std::string MemClientStub::getDeviceDescription(const std::string& name)
{
	NUT_UNUSED_VARIABLE(name);
	throw NutException("Not implemented");
}

std::set<std::string> MemClientStub::getDeviceVariableNames(const std::string& dev)
{
	NUT_UNUSED_VARIABLE(dev);
	throw NutException("Not implemented");
}

std::set<std::string> MemClientStub::getDeviceRWVariableNames(const std::string& dev)
{
	NUT_UNUSED_VARIABLE(dev);
	throw NutException("Not implemented");
}

std::string MemClientStub::getDeviceVariableDescription(const std::string& dev, const std::string& name)
{
	NUT_UNUSED_VARIABLE(dev);
	NUT_UNUSED_VARIABLE(name);
	throw NutException("Not implemented");
}

ListValue MemClientStub::getDeviceVariableValue(const std::string& dev, const std::string& name)
{
	ListValue res;
	auto it_dev = _values.find(dev);
	if (it_dev != _values.end())
	{
		auto map = it_dev->second;
		auto it_map = map.find(name);
		if (it_map != map.end())
		{
			res = it_map->second;
		}
	}
	return res;
}

ListObject MemClientStub::getDeviceVariableValues(const std::string& dev)
{
	ListObject res;
	auto it_dev = _values.find(dev);
	if (it_dev != _values.end())
	{
		res = it_dev->second;
	}
	return res;
}

ListDevice MemClientStub::getDevicesVariableValues(const std::set<std::string>& devs)
{
	ListDevice res;
	for (auto itr = devs.begin(); itr != devs.end(); itr++)
	{
		std::string dev = *itr;
		auto it_dev = _values.find(dev);
		if (it_dev != _values.end())
		{
			res.insert(std::pair<std::string, ListObject>(dev, it_dev->second));
		}
	}
	return res;
}

TrackingID MemClientStub::setDeviceVariable(const std::string& dev, const std::string& name, const std::string& value)
{
	auto it_dev = _values.find(dev);
	if (it_dev == _values.end())
	{
		ListObject list;
		_values.emplace(dev, list);
		it_dev = _values.find(dev);
	}
	if (it_dev != _values.end())
	{
		auto map = &(it_dev->second);
		auto it_map = map->find(name);
		if (it_map != map->end())
		{
			it_map->second[0] = value;
		}
		else
		{
			ListValue list_value;
			list_value.push_back(value);
			map->emplace(name, list_value);
		}
	}
	return "";
}

TrackingID MemClientStub::setDeviceVariable(const std::string& dev, const std::string& name, const ListValue& values)
{
	auto it_dev = _values.find(dev);
	if (it_dev != _values.end())
	{
		auto map = &(it_dev->second);
		auto it_map = map->find(name);
		if (it_map != map->end())
		{
			it_map->second = values;
		}
		else
		{
			map->emplace(name, values);
		}
	}
	return "";
}

std::set<std::string> MemClientStub::getDeviceCommandNames(const std::string& dev)
{
	NUT_UNUSED_VARIABLE(dev);
	throw NutException("Not implemented");
}

std::string MemClientStub::getDeviceCommandDescription(const std::string& dev, const std::string& name)
{
	NUT_UNUSED_VARIABLE(dev);
	NUT_UNUSED_VARIABLE(name);
	throw NutException("Not implemented");
}

TrackingID MemClientStub::executeDeviceCommand(const std::string& dev, const std::string& name, const std::string& param)
{
	NUT_UNUSED_VARIABLE(dev);
	NUT_UNUSED_VARIABLE(name);
	NUT_UNUSED_VARIABLE(param);
	throw NutException("Not implemented");
}

std::map<std::string, std::set<std::string>> MemClientStub::listDeviceClients(void)
{
	throw NutException("Not implemented");
}

std::set<std::string> MemClientStub::deviceGetClients(const std::string& dev)
{
	NUT_UNUSED_VARIABLE(dev);
	throw NutException("Not implemented");
}

void MemClientStub::deviceLogin(const std::string& dev)
{
	NUT_UNUSED_VARIABLE(dev);
	throw NutException("Not implemented");
}

/* Note: "master" is deprecated, but supported
 * for mixing old/new client/server combos: */
void MemClientStub::deviceMaster(const std::string& dev)
{
	NUT_UNUSED_VARIABLE(dev);
	throw NutException("Not implemented");
}

void MemClientStub::devicePrimary(const std::string& dev)
{
	NUT_UNUSED_VARIABLE(dev);
	throw NutException("Not implemented");
}

void MemClientStub::deviceForcedShutdown(const std::string& dev)
{
	NUT_UNUSED_VARIABLE(dev);
	throw NutException("Not implemented");
}

int MemClientStub::deviceGetNumLogins(const std::string& dev)
{
	NUT_UNUSED_VARIABLE(dev);
	throw NutException("Not implemented");
}

TrackingResult MemClientStub::getTrackingResult(const TrackingID& id)
{
	NUT_UNUSED_VARIABLE(id);
	throw NutException("Not implemented");
	//return TrackingResult::SUCCESS;
}

bool MemClientStub::isFeatureEnabled(const Feature& feature)
{
	NUT_UNUSED_VARIABLE(feature);
	throw NutException("Not implemented");
}

void MemClientStub::setFeature(const Feature& feature, bool status)
{
	NUT_UNUSED_VARIABLE(feature);
	NUT_UNUSED_VARIABLE(status);
	throw NutException("Not implemented");
}

} /* namespace nut */

/**
 * C nutclient API.
 */
extern "C" {

NUTCLIENT_MEM_t nutclient_mem_create_client()
{
	nut::MemClientStub* client = new nut::MemClientStub;
	try
	{
		return static_cast<NUTCLIENT_MEM_t>(client);
	}
	catch(nut::NutException& ex)
	{
		// TODO really catch it
		NUT_UNUSED_VARIABLE(ex);
		delete client;
		return nullptr;
	}
}

} /* extern "C" */
