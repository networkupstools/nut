#!@PYTHON@
# -*- coding: utf-8 -*-

#   Copyright (C) 2008 David Goncalves <david@lestat.st>
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.

# 2008-01-14 David Goncalves
#            PyNUT is an abstraction class to access NUT (Network UPS Tools) server.
#
# 2008-06-09 David Goncalves
#            Added 'GetRWVars' and 'SetRWVar' commands.
#
# 2009-02-19 David Goncalves
#            Changed class PyNUT to PyNUTClient
#
# 2010-07-23 David Goncalves - Version 1.2
#            Changed GetRWVars function that fails is the UPS is not
#            providing such vars.
#
# 2011-07-05 René Martín Rodríguez <rmrodri@ull.es> - Version 1.2.1
#            Added support for FSD, HELP and VER commands
#
# 2012-02-07 René Martín Rodríguez <rmrodri@ull.es> - Version 1.2.2
#            Added support for LIST CLIENTS command
#
# 2014-06-03 george2 - Version 1.3.0
#            Added custom exception class, fixed minor bug, added Python 3 support.
#
# 2021-09-27 Jim Klimov <jimklimov+nut@gmail.com> - Version 1.4.0
#            Revise strings used to be byte sequences as required by telnetlib
#            in Python 3.9, by spelling out b"STR" or str.encode('ascii');
#            the change was also tested to work with Python 2.7, 3.4, 3.5 and
#            3.7 (to the extent of accompanying test_nutclient.py at least).
#
# 2022-08-12 Jim Klimov <jimklimov+nut@gmail.com> - Version 1.5.0
#            Fix ListClients() method to actually work with current NUT protocol
#            Added DeviceLogin() method
#            Added GetUPSNames() method
#            Fixed raised PyNUTError() exceptions to carry a Python string
#            (suitable for Python2 and Python3), not byte array from protocol,
#            so exception catchers can process them naturally (see test script).
#
# 2023-01-18 Jim Klimov <jimklimov+nut@gmail.com> - Version 1.6.0
#            Added CheckUPSAvailable() method originally by Michal Hlavinka
#            from 2013-01-07 RedHat/Fedora packaging

import telnetlib

class PyNUTError( Exception ) :
    """ Base class for custom exceptions """


class PyNUTClient :
    """ Abstraction class to access NUT (Network UPS Tools) server """

    __debug       = None   # Set class to debug mode (prints everything useful for debuging...)
    __host        = None
    __port        = None
    __login       = None
    __password    = None
    __timeout     = None
    __srv_handler = None

    __version     = "1.6.0"
    __release     = "2023-01-18"


    def __init__( self, host="127.0.0.1", port=3493, login=None, password=None, debug=False, timeout=5 ) :
        """ Class initialization method

host     : Host to connect (default to localhost)
port     : Port where NUT listens for connections (default to 3493)
login    : Login used to connect to NUT server (default to None for no authentication)
password : Password used when using authentication (default to None)
debug    : Boolean, put class in debug mode (prints everything on console, default to False)
timeout  : Timeout used to wait for network response
        """
        self.__debug = debug

        if self.__debug :
            print( "[DEBUG] Class initialization..." )
            print( "[DEBUG]  -> Host  = %s (port %s)" % ( host, port ) )
            print( "[DEBUG]  -> Login = '%s' / '%s'" % ( login, password ) )

        self.__host     = host
        self.__port     = port
        self.__login    = login
        self.__password = password
        self.__timeout  = 5

        self.__connect()

    # Try to disconnect cleanly when class is deleted ;)
    def __del__( self ) :
        """ Class destructor method """
        try :
            self.__srv_handler.write( b"LOGOUT\n" )
        except :
            pass

    def __connect( self ) :
        """ Connects to the defined server

If login/pass was specified, the class tries to authenticate. An error is raised
if something goes wrong.
        """
        if self.__debug :
            print( "[DEBUG] Connecting to host" )

        self.__srv_handler = telnetlib.Telnet( self.__host, self.__port )

        if self.__login != None :
            self.__srv_handler.write( ("USERNAME %s\n" % self.__login).encode('ascii') )
            result = self.__srv_handler.read_until( b"\n", self.__timeout )
            if result[:2] != b"OK" :
                raise PyNUTError( result.replace( b"\n", b"" ).decode('ascii') )

        if self.__password != None :
            self.__srv_handler.write( ("PASSWORD %s\n" % self.__password).encode('ascii') )
            result = self.__srv_handler.read_until( b"\n", self.__timeout )
            if result[:2] != b"OK" :
                if result == b"ERR INVALID-ARGUMENT\n" :
                    # Quote the password (if it has whitespace etc)
                    # TODO: Escape special chard like NUT does?
                    self.__srv_handler.write( ("PASSWORD \"%s\"\n" % self.__password).encode('ascii') )
                    result = self.__srv_handler.read_until( b"\n", self.__timeout )
                    if result[:2] != b"OK" :
                        raise PyNUTError( result.replace( b"\n", b"" ).decode('ascii') )
                else:
                    raise PyNUTError( result.replace( b"\n", b"" ).decode('ascii') )

    def GetUPSList( self ) :
        """ Returns the list of available UPS from the NUT server

The result is a dictionary containing 'key->val' pairs of 'UPSName' and 'UPS Description'

Note that fields here are byte sequences (not locale-aware strings)
which is of little concern for Python2 but is important in Python3
(e.g. when we use "str" type `ups` variables or check their "validity").
        """
        if self.__debug :
            print( "[DEBUG] GetUPSList from server" )

        self.__srv_handler.write( b"LIST UPS\n" )
        result = self.__srv_handler.read_until( b"\n" )
        if result != b"BEGIN LIST UPS\n" :
            raise PyNUTError( result.replace( b"\n", b"" ).decode('ascii') )

        result = self.__srv_handler.read_until( b"END LIST UPS\n" )
        ups_list = {}

        for line in result.split( b"\n" ) :
            if line[:3] == b"UPS" :
                ups, desc = line[4:-1].split( b'"' )
                ups_list[ ups.replace( b" ", b"" ) ] = desc

        return( ups_list )

    def GetUPSNames( self ) :
        """ Returns the list of available UPS names from the NUT server as strings

The result is a set of str objects (comparable with ups="somename" and
useful as arguments to other methods). Helps work around Python2/Python3
string API changes.
        """
        if self.__debug :
            print( "[DEBUG] GetUPSNames from server" )

        self_ups_list = []
        for b in self.GetUPSList():
            self_ups_list.append(b.decode('ascii'))

        return self_ups_list

    def GetUPSVars( self, ups="" ) :
        """ Get all available vars from the specified UPS

The result is a dictionary containing 'key->val' pairs of all
available vars.
        """
        if self.__debug :
            print( "[DEBUG] GetUPSVars called..." )

        self.__srv_handler.write( ("LIST VAR %s\n" % ups).encode('ascii') )
        result = self.__srv_handler.read_until( b"\n" )
        if result != ("BEGIN LIST VAR %s\n" % ups).encode('ascii') :
            raise PyNUTError( result.replace( b"\n", b"" ).decode('ascii') )

        ups_vars   = {}
        result     = self.__srv_handler.read_until( ("END LIST VAR %s\n" % ups).encode('ascii') )
        offset     = len( ("VAR %s " % ups ).encode('ascii') )
        end_offset = 0 - ( len( ("END LIST VAR %s\n" % ups).encode('ascii') ) + 1 )

        for current in result[:end_offset].split( b"\n" ) :
            var  = current[ offset: ].split( b'"' )[0].replace( b" ", b"" )
            data = current[ offset: ].split( b'"' )[1]
            ups_vars[ var ] = data

        return( ups_vars )

    def CheckUPSAvailable( self, ups="" ) :
        """ Check whether UPS is reachable

Just tries to contact UPS with a safe command.
The result is True (reachable) or False (unreachable)
        """
        if self.__debug :
            print( "[DEBUG] CheckUPSAvailable called..." )

        self.__srv_handler.write( ("LIST CMD %s\n" % ups).encode('ascii') )
        result = self.__srv_handler.read_until( b"\n" )
        if result != ("BEGIN LIST CMD %s\n" % ups).encode('ascii') :
            return False

        self.__srv_handler.read_until( ("END LIST CMD %s\n" % ups).encode('ascii') )
        return True

    def GetUPSCommands( self, ups="" ) :
        """ Get all available commands for the specified UPS

The result is a dict object with command name as key and a description
of the command as value
        """
        if self.__debug :
            print( "[DEBUG] GetUPSCommands called..." )

        self.__srv_handler.write( ("LIST CMD %s\n" % ups).encode('ascii') )
        result = self.__srv_handler.read_until( b"\n" )
        if result != ("BEGIN LIST CMD %s\n" % ups).encode('ascii') :
            raise PyNUTError( result.replace( b"\n", b"" ).decode('ascii') )

        ups_cmds   = {}
        result     = self.__srv_handler.read_until( ("END LIST CMD %s\n" % ups).encode('ascii') )
        offset     = len( ("CMD %s " % ups).encode('ascii') )
        end_offset = 0 - ( len( ("END LIST CMD %s\n" % ups).encode('ascii') ) + 1 )

        for current in result[:end_offset].split( b"\n" ) :
            var  = current[ offset: ].split( b'"' )[0].replace( b" ", b"" )

            # For each var we try to get the available description
            try :
                self.__srv_handler.write( ("GET CMDDESC %s %s\n" % ( ups, var )).encode('ascii') )
                temp = self.__srv_handler.read_until( b"\n" )
                if temp[:7] != b"CMDDESC" :
                    raise PyNUTError
                else :
                    off  = len( ("CMDDESC %s %s " % ( ups, var )).encode('ascii') )
                    desc = temp[off:-1].split(b'"')[1]
            except :
                desc = var

            ups_cmds[ var ] = desc

        return( ups_cmds )

    def GetRWVars( self,  ups="" ) :
        """ Get a list of all writable vars from the selected UPS

The result is presented as a dictionary containing 'key->val' pairs
        """
        if self.__debug :
            print( "[DEBUG] GetUPSVars from '%s'..." % ups )

        self.__srv_handler.write( ("LIST RW %s\n" % ups).encode('ascii') )
        result = self.__srv_handler.read_until( b"\n" )
        if ( result != ("BEGIN LIST RW %s\n" % ups).encode('ascii') ) :
            raise PyNUTError( result.replace( b"\n", b"" ).decode('ascii') )

        result     = self.__srv_handler.read_until( ("END LIST RW %s\n" % ups).encode('ascii') )
        offset     = len( ("VAR %s" % ups).encode('ascii') )
        end_offset = 0 - ( len( ("END LIST RW %s\n" % ups).encode('ascii') ) + 1 )
        rw_vars    = {}

        try :
            for current in result[:end_offset].split( b"\n" ) :
                var  = current[ offset: ].split( b'"' )[0].replace( b" ", b"" )
                data = current[ offset: ].split( b'"' )[1]
                rw_vars[ var ] = data

        except :
            pass

        return( rw_vars )

    def SetRWVar( self, ups="", var="", value="" ):
        """ Set a variable to the specified value on selected UPS

The variable must be a writable value (cf GetRWVars) and you must have the proper
rights to set it (maybe login/password).
        """

        self.__srv_handler.write( ("SET VAR %s %s %s\n" % ( ups, var, value )).encode('ascii') )
        result = self.__srv_handler.read_until( b"\n" )
        if ( result == b"OK\n" ) :
            return( "OK" )
        else :
            raise PyNUTError( result.replace( b"\n", b"" ).decode('ascii') )

    def RunUPSCommand( self, ups="", command="" ) :
        """ Send a command to the specified UPS

Returns OK on success or raises an error
        """

        if self.__debug :
            print( "[DEBUG] RunUPSCommand called..." )

        self.__srv_handler.write( ("INSTCMD %s %s\n" % ( ups, command )).encode('ascii') )
        result = self.__srv_handler.read_until( b"\n" )
        if ( result == b"OK\n" ) :
            return( "OK" )
        else :
            raise PyNUTError( result.replace( b"\n", b"" ).decode('ascii') )

    def DeviceLogin( self, ups="") :
        """ Establish a login session with a device (like upsmon does)

Returns OK on success or raises an error
USERNAME and PASSWORD must have been specified earlier in the session (once)
and upsd.conf should permit that user with one of `upsmon` role types.

Note there is no "device LOGOUT" in the protocol, just one for general end
of connection.
        """

        if self.__debug :
            print( "[DEBUG] DeviceLogin called..." )

        if ups is None or (ups not in self.GetUPSNames()):
            if self.__debug :
                print( "[DEBUG] DeviceLogin: %s is not a valid UPS" % ups )
            raise PyNUTError( "ERR UNKNOWN-UPS" )

        self.__srv_handler.write( ("LOGIN %s\n" % ups).encode('ascii') )
        result = self.__srv_handler.read_until( b"\n" )
        if ( result.startswith( ("User %s@" % self.__login).encode('ascii')) and result.endswith (("[%s]\n" % ups).encode('ascii')) ):
            # User dummy-user@127.0.0.1 logged into UPS [dummy]
            # Read next line then
            result = self.__srv_handler.read_until( b"\n" )
        if ( result == b"OK\n" ) :
            return( "OK" )
        else :
            raise PyNUTError( result.replace( b"\n", b"" ).decode('ascii') )

    def FSD( self, ups="") :
        """ Send FSD command

Returns OK on success or raises an error

NOTE: API changed since NUT 2.8.0 to replace MASTER with PRIMARY
(and backwards-compatible alias handling)
        """

        if self.__debug :
            print( "[DEBUG] PRIMARY called..." )

        self.__srv_handler.write( ("PRIMARY %s\n" % ups).encode('ascii') )
        result = self.__srv_handler.read_until( b"\n" )
        if ( result != b"OK PRIMARY-GRANTED\n" ) :
            if self.__debug :
                print( "[DEBUG] Retrying: MASTER called..." )
            self.__srv_handler.write( ("MASTER %s\n" % ups).encode('ascii') )
            result = self.__srv_handler.read_until( b"\n" )
            if ( result != b"OK MASTER-GRANTED\n" ) :
                if self.__debug :
                    print( "[DEBUG] Primary level functions are not available" )
                raise PyNUTError( "ERR ACCESS-DENIED" )

        if self.__debug :
            print( "[DEBUG] FSD called..." )
        self.__srv_handler.write( ("FSD %s\n" % ups).encode('ascii') )
        result = self.__srv_handler.read_until( b"\n" )
        if ( result == b"OK FSD-SET\n" ) :
            return( "OK" )
        else :
            raise PyNUTError( result.replace( b"\n", b"" ).decode('ascii') )

    def help(self) :
        """ Send HELP command
        """

        if self.__debug :
            print( "[DEBUG] HELP called..." )

        self.__srv_handler.write( b"HELP\n" )
        return self.__srv_handler.read_until( b"\n" )

    def ver(self) :
        """ Send VER command
        """

        if self.__debug :
            print( "[DEBUG] VER called..." )

        self.__srv_handler.write( b"VER\n" )
        return self.__srv_handler.read_until( b"\n" )

    def ListClients( self, ups = None ) :
        """ Returns the list of connected clients from the NUT server

The result is a dictionary containing 'key->val' pairs of 'UPSName' and a list of clients
        """
        if self.__debug :
            print( "[DEBUG] ListClients from server: %s" % ups )

        # If (!ups) we use this list below to recurse:
        self_ups_list = self.GetUPSNames()
        if ups and (ups not in self_ups_list):
            if self.__debug :
                print( "[DEBUG] ListClients: %s is not a valid UPS" % ups )
            raise PyNUTError( "ERR UNKNOWN-UPS" )

        if ups:
            self.__srv_handler.write( ("LIST CLIENT %s\n" % ups).encode('ascii') )
        else:
            # NOTE: Currently NUT does not support just listing all clients
            # (not providing an "ups" argument) => NUT_ERR_INVALID_ARGUMENT
            self.__srv_handler.write( b"LIST CLIENT\n" )
        result = self.__srv_handler.read_until( b"\n" )
        if ( (ups and result != ("BEGIN LIST CLIENT %s\n" % ups).encode('ascii')) or (ups is None and result != b"BEGIN LIST CLIENT\n") ):
            if ups is None and (result == b"ERR INVALID-ARGUMENT\n") :
                # For ups==None, list all upses, list their clients
                if self.__debug :
                    print( "[DEBUG] Recurse ListClients() because it did not specify one UPS to query" )
                ups_list = {}
                for ups in self_ups_list :
                    # Update "ups_list" dict with contents of recursive call return
                    ups_list.update(self.ListClients(ups))
                return( ups_list )

            # had a seemingly valid arg, but no success:
            if self.__debug :
                print( "[DEBUG] ListClients from server got unexpected result: %s" % result )

            raise PyNUTError( result.replace( b"\n", b"" ).decode('ascii') )

        if ups :
            result = self.__srv_handler.read_until( ("END LIST CLIENT %s\n" % ups).encode('ascii') )
        else:
            # Should not get here with current NUT:
            result = self.__srv_handler.read_until( b"END LIST CLIENT\n" )
        ups_list = {}

        for line in result.split( b"\n" ):
            ###print( "[DEBUG] ListClients line: '%s'" % line )
            if line[:6] == b"CLIENT" :
                ups, host = line[7:].split(b' ')
                ups.replace(b' ', b'')
                host.replace(b' ', b'')
                if not ups in ups_list:
                    ups_list[ups] = []
                ups_list[ups].append(host)

        return( ups_list )
