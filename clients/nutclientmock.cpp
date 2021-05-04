/* nutclientmock.cpp - nutclientmock C++ library implementation

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

#include "nutclientmock.h"

namespace nut
{

/*
 *
 * TCP Client Mock implementation
 *
 */

Device TcpClientMock::getDevice(const std::string& name)
{
	throw NutException("Not implemented");
}

std::set<std::string> TcpClientMock::getDeviceNames()
{
	throw NutException("Not implemented");
}

std::string TcpClientMock::getDeviceDescription(const std::string& name)
{
	throw NutException("Not implemented");
}

std::set<std::string> TcpClientMock::getDeviceVariableNames(const std::string& dev)
{
	throw NutException("Not implemented");
}

std::set<std::string> TcpClientMock::getDeviceRWVariableNames(const std::string& dev)
{
	throw NutException("Not implemented");
}

std::string TcpClientMock::getDeviceVariableDescription(const std::string& dev, const std::string& name)
{
	throw NutException("Not implemented");
}

std::vector<std::string> TcpClientMock::getDeviceVariableValue(const std::string& dev, const std::string& name)
{
	std::vector<std::string> res;
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

std::map<std::string, std::vector<std::string>> TcpClientMock::getDeviceVariableValues(const std::string& dev)
{
	std::map<std::string, std::vector<std::string>> res;
	auto it_dev = _values.find(dev);
	if (it_dev != _values.end())
	{
		res = it_dev->second;
	}
	return res;
}

std::map<std::string, std::map<std::string, std::vector<std::string>>> TcpClientMock::getDevicesVariableValues(const std::set<std::string>& devs)
{
	std::map<std::string, std::map<std::string, std::vector<std::string>>> res;

	for (auto itr = devs.begin(); itr != devs.end(); itr++)
	{
		std::string dev = *itr;
		auto it_dev = _values.find(dev);
		if (it_dev != _values.end())
		{
			res.insert(std::pair<std::string, std::map<std::string, std::vector<std::string>>>(dev, it_dev->second));
		}
	}
	return res;
}

TrackingID TcpClientMock::setDeviceVariable(const std::string& dev, const std::string& name, const std::string& value)
{
	auto it_dev = _values.find(dev);
	if (it_dev == _values.end())
	{
		std::map<std::string, std::vector<std::string>> list;
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
			std::vector<std::string> list_value;
			list_value.push_back(value);
			map->emplace(name, list_value);
		}
	}
	return "";
}

TrackingID TcpClientMock::setDeviceVariable(const std::string& dev, const std::string& name, const std::vector<std::string>& values)
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

std::set<std::string> TcpClientMock::getDeviceCommandNames(const std::string& dev)
{
	throw NutException("Not implemented");
}

std::string TcpClientMock::getDeviceCommandDescription(const std::string& dev, const std::string& name)
{
	throw NutException("Not implemented");
}

TrackingID TcpClientMock::executeDeviceCommand(const std::string& dev, const std::string& name, const std::string& param)
{
	throw NutException("Not implemented");
}

void TcpClientMock::deviceLogin(const std::string& dev)
{
	throw NutException("Not implemented");
}

void TcpClientMock::deviceMaster(const std::string& dev)
{
	throw NutException("Not implemented");
}

void TcpClientMock::deviceForcedShutdown(const std::string& dev)
{
	throw NutException("Not implemented");
}

int TcpClientMock::deviceGetNumLogins(const std::string& dev)
{
	throw NutException("Not implemented");
}

TrackingResult TcpClientMock::getTrackingResult(const TrackingID& id)
{
	throw NutException("Not implemented");
	//return TrackingResult::SUCCESS;
}

bool TcpClientMock::isFeatureEnabled(const Feature& feature)
{
	throw NutException("Not implemented");
}
void TcpClientMock::setFeature(const Feature& feature, bool status)
{
	throw NutException("Not implemented");
}

} /* namespace nut */


