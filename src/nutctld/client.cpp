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

#include "libnutctl.hpp"
using namespace org::networkupstools;


NutController* ctl = NULL;

int main(int argc, char** argv)
{
	cout << "nutctl client" << endl;

	ctl = NutController::getController();

	ctl->ScanUSB();
	ctl->ScanAvahi(1000000);
	ctl->ScanNut("166.99.224.1", "166.99.224.254", 3493, 1000000);
	ctl->ScanSNMPv1("166.99.224.1", "166.99.224.254", 1000000, "public");
//	ctl->ScanSNMPv3("166.99.224.1", "166.99.224.254", 1000000, "readuser", 1, "MD5", "readuser", "toto", "toto");
	ctl->ScanXMLHTTP(1000000);

	std::vector< std::string > res = ctl->GetDeviceNames();
	for(size_t n=0; n<res.size(); n++)
	{
		cout << " - " << res[n] << endl;
		std::map< std::string, std::string > vars = ctl->GetDevice(res[n]);
		for(std::map< std::string, std::string >::iterator it=vars.begin(); it!=vars.end(); ++it)
		{
			cout << "    - " << it->first << " : " << it->second << std::endl;
		}
	}

	return 0;
}
