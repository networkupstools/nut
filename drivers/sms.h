/* sms.h - model capability table

   This code is derived from Russell Kroll <rkroll@exploits.org>,
   Fenton UPS Driver

   Copyright (C) 2001  Marcio Gomes  <tecnica@microlink.com.br>
   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>

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

   Not possibile only add SMS models to fentonups, the id strings are
   recived from ups in different orders.. 

   2001/05/17 - Version 0.10 - Initial release
   2001/06/01 - Version 0.20 - Add Battery Informations in driver
   2001/06/04 - Version 0.30 - Updated Battery Volts range, to reflect a correct                               percent ( % )
   2002/12/02 - Version 0.40 - Update driver to new-model, based on Fentonups
                               driver version 0.90
   2002/12/18 - Version 0.50 - Add Sinus Single 2 KVA in database, test with
                               new Manager III sinusoidal versions.
                               Change Detect Name, SMS do not pass real Models
                               in Megatec Info command, only the VA/KVA version
   2002/12/18 - Version 0.51 - Updated Battery Volts range, to reflect a correct                               percent ( % )
   2002/12/27 - Version 0.60 - Add new UPS Commands SDRET, SIMPWF change BTEST1
   2002/12/28 - Version 0.70 - Add new UPS Commands SHUTDOWN,STOPSHUTD,WATCHDOG

   Microlink ISP/Pop-Rio contributed with MANAGER III 1300, MANAGER III 650 UPS
   and Sinus Single 2 KVA for my tests.

   http://www.microlink.com.br and http://www.pop-rio.com.br  


*/

struct {
	const	char	*mtext;
	const	char	*desc;
	float	lowvolt;
	float	voltrange;
	int	lowxfer;
	int	lownorm;
	int	highnorm;
	int	highxfer;
	int	has_temp;
}	

/*

#mtext lowvolt voltrange lowxfer lownorm highxfer highnorm 


        0         1         2        3
        012345678901234567890123567890123456789
        #SMS LTDA         1300 VA   VER 1.0
        #SMS LTDA        1300VA SEN VER 5.0
        #SMS LTDA             2 KVA   VER 1.0

Manager III 650 Senoidal are detected like Manager III 1300 VA Senoidal
UPS.  I think are problems in Ver 5.0 SMS Firmware 

*/



       modeltab[] = 
{
{" 650",      "Manager III 650 VA" ,             9.6, 3.8, 86, 105, 133, 140,1},
{" 1300 VA",  "Manager III 1300 VA",             9.6, 3.8, 86, 105, 133, 140,1},
{"1300VA SEN","Manager III 650/1300VA Senoidal", 9.6, 3.7, 86, 105, 133, 140,1},
{"     2 KVA","Sinus Single Senoidal 2 KVA",     9.6, 3.7, 86, 105, 133, 140,1},
{ NULL,    NULL,		  0, 0,   0,   0,   0,   0, 0 }
};
