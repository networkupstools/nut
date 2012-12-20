
#include "nutconf.h"
#include "nutstream.h"
using namespace nut;

#include <iostream>
using namespace std;

int main(int argc, char** argv)
{
	cout << "nutconf testing program" << endl;

	if(argc<2)
	{
		cout << "Usage: nutconf {ups.conf path}" << endl;
		return 0;
	}

	// 
	// UPS.CONF
	//
	if(argc>=2)
	{
		cout << endl << "ups.conf:" << endl << endl;

		GenericConfiguration ups_conf;
		NutFile file(argv[1]);
		file.open();
		ups_conf.parseFrom(file);

		for(std::map<std::string, GenericConfigSection>::iterator it = ups_conf.sections.begin();
			it != ups_conf.sections.end(); ++it)
		{
			GenericConfigSection& section = it->second;
			cout << "[" << section.name << "]" << endl;

			for(GenericConfigSection::EntryMap::iterator iter = section.entries.begin();
				iter != section.entries.end(); ++iter)
			{
				cout << "  " << iter->first << " = " << iter->second.values.front() << endl;
			}
		}
	}

	//
	// UPSD.CONF 
	//
	if(argc>=3)
	{
		cout << endl << "upsd.conf:" << endl << endl;

		UpsdConfiguration upsd_conf;
		NutFile file(argv[2]);
		file.open();
		upsd_conf.parseFrom(file);

		cout << "maxAge: " << upsd_conf.maxAge << endl;
		cout << "maxConn: " << upsd_conf.maxConn << endl;
		cout << "statePath: " << *upsd_conf.statePath << endl;
		cout << "certFile: " << *upsd_conf.certFile << endl;
		cout << "listen:" << endl;

		for(std::list<UpsdConfiguration::Listen>::iterator it=upsd_conf.listens.begin();
			it!=upsd_conf.listens.end(); ++it)
		{
			cout << " - " << it->address << ":" << it->port << endl;
		}
	}

	return 0;
	

}

