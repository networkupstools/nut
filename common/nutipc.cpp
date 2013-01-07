/* nutipc.cpp - NUT IPC

   Copyright (C) 2012 Eaton

   Author: Vaclav Krpec  <VaclavKrpec@Eaton.com>

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

#include "nutipc.hpp"


namespace nut {

pid_t Process::getPID() throw() {
	return getpid();
}


pid_t Process::getPPID() throw() {
	return getppid();
}


Process::Executor::Executor(const std::string & command) {
	//m_bin m_args
	throw std::runtime_error("TODO: Not implemented, yet");
}


int Process::Executor::operator () () throw(std::runtime_error) {
        const char ** args_c_str = new const char *[m_args.size() + 2];

	const char * bin_c_str = m_bin.c_str();

	args_c_str[0] = bin_c_str;

	Arguments::const_iterator arg = m_args.begin();

	size_t i = 1;

	for (; arg != m_args.end(); ++arg, ++i) {
		args_c_str[i] = (*arg).c_str();
	}

	args_c_str[i] = NULL;

	int status = ::execvp(bin_c_str, (char * const *)args_c_str);

	// Upon successful execution, the execvp function never returns
	// (since the process context is replaced, completely)

	delete args_c_str;

	std::stringstream e;

	e << "Failed to execute binary " << m_bin << ": " << status;

	throw std::runtime_error(e.str());
}


int writeCommand(int fh, void * cmd, size_t cmd_size) throw(std::runtime_error) {
	char * cmd_bytes = reinterpret_cast<char *>(cmd);

	do {
		ssize_t written = write(fh, cmd_bytes, cmd_size);

		if (-1 == written)
			return errno;

		cmd_bytes += written;
		cmd_size  -= written;

	} while (cmd_size);

	return 0;
}


int Signal::send(Signal::enum_t signame, pid_t pid) throw(std::logic_error) {
	int sig = (int)signame;

	int status = ::kill(pid, sig);

	if (0 == status)
		return 0;

	if (EINVAL != errno)
		return errno;

	std::stringstream e;

	e << "Can't send invalid signal " << sig;

	throw std::logic_error(e.str());
}

}  // end of namespace nut
