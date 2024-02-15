/*
    nutipc.cpp - NUT IPC

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

#include "config.h"

/* For C++ code below, we do not actually use the fallback time methods
 * (on mingw mostly), but in C++ context they happen to conflict with
 * time.h or ctime headers, while native-C does not. Just disable the
 * fallback localtime_r(), gmtime_r() etc. if/when NUT common.h gets
 * included by the header chain:
 */
#ifndef HAVE_GMTIME_R
# define HAVE_GMTIME_R 111
#endif
#ifndef HAVE_LOCALTIME_R
# define HAVE_LOCALTIME_R 111
#endif

#include "nutipc.hpp"
#include "nutstream.hpp"

#include <iostream>


namespace nut {

/* Trivial implementations out of class declaration to avoid
 * error: 'ClassName' has no out-of-line virtual method definitions; its vtable
 *   will be emitted in every translation unit [-Werror,-Wweak-vtables]
 */
Process::Main::~Main() {}
Signal::Handler::~Handler() {}

pid_t Process::getPID()
#if (defined __cplusplus) && (__cplusplus < 201100)
	throw()
#endif
{
	return getpid();
}


pid_t Process::getPPID()
#if (defined __cplusplus) && (__cplusplus < 201100)
	throw()
#endif
{
#ifdef WIN32
	/* FIXME: Detect HAVE_GETPPID in configure; throw exceptions here?..
	 * NOTE: Does not seem to be currently used in nutconf codebase. */
	return -1;
#else
	return getppid();
#endif
}


/**
 *  \brief  Command line segmentation
 *
 *  The function parses the \c command and chops off (and return)
 *  the first command line word (i.e. does segmentation based
 *  on white spaces, unless quoted).
 *  White spaces are removed from the returned words.
 *
 *  \param[in,out]  command  Command line
 *
 *  \return Command line word
 */
static std::string getCmdLineWord(std::string & command) {
	size_t len = 0;

	// Remove initial whitespace
	while (len < command.size()) {
		if (' ' != command[len] && '\t' != command[len])
			break;

		++len;
	}

	command.erase(0, len);

	// Seek word end
	bool bslsh = false;
	char quote = 0;

	for (len = 0; len < command.size(); ++len) {
		char ch = command[len];

		// White space (may be inside quotes)
		if (' ' == ch || '\t' == ch) {
			if (!quote)
				break;
		}

		// Backspace (second one cancels the first)
		else if ('\\' == ch) {
			bslsh = bslsh ? false : true;
		}

		// Double quote (may be escaped or nested)
		else if ('"' == ch) {
			if (!bslsh) {
				if (!quote)
					quote = '"';

				// Final double quote
				else if ('"' == quote)
					quote = 0;
			}
		}

		// Single quote (can't be escaped)
		else if ('\'' == ch) {
			if (!quote)
				quote = '\'';

			else if ('\'' == quote)
				quote = 0;
		}

		// Cancel backslash
		if ('\\' != ch)
			bslsh = false;
	}

	// Extract the word
	std::string word = command.substr(0, len);

	command.erase(0, len);

	return word;
}


Process::Executor::Executor(const std::string & command) {
	std::string cmd(command);

	m_bin = getCmdLineWord(cmd);

	for (;;) {
		std::string arg = getCmdLineWord(cmd);

		if (arg.empty())
			break;

		m_args.push_back(arg);
	}
}


int Process::Executor::operator () ()
#if (defined __cplusplus) && (__cplusplus < 201100)
	throw(std::runtime_error)
#endif
{
	const char ** args_c_str = new const char *[m_args.size() + 2];

	const char * bin_c_str = m_bin.c_str();

	args_c_str[0] = bin_c_str;

	Arguments::const_iterator arg = m_args.begin();

	size_t i = 1;

	for (; arg != m_args.end(); ++arg, ++i) {
		args_c_str[i] = (*arg).c_str();
	}

	args_c_str[i] = nullptr;

	int status = ::execvp(bin_c_str, const_cast<char * const *>(args_c_str));

	// Upon successful execution, the execvp function never returns
	// (since the process context is replaced, completely)

	delete[] args_c_str;

	std::stringstream e;

	e << "Failed to execute binary " << m_bin << ": " << status;

	throw std::runtime_error(e.str());
}


int sigPipeWriteCmd(int fh, void * cmd, size_t cmd_size)
#if (defined __cplusplus) && (__cplusplus < 201100)
	throw(std::runtime_error)
#endif
{
	char * cmd_bytes = reinterpret_cast<char *>(cmd);

	do {
		ssize_t written = ::write(fh, cmd_bytes, cmd_size);

		if (written < 0)
			return errno;

		cmd_bytes += written;
		cmd_size  -= static_cast<size_t>(written);

	} while (cmd_size);

	return 0;
}


int Signal::send(Signal::enum_t signame, pid_t pid)
#if (defined __cplusplus) && (__cplusplus < 201100)
	throw(std::logic_error)
#endif
{
	int sig = static_cast<int>(signame);
#ifdef WIN32
	/* FIXME: Implement (for NUT processes) via pipes?
	 * See e.g. upsdrvctl implementation. */
	std::stringstream e;

	e << "Can't send signal " << sig << " to PID " << pid <<
			": not implemented on this platform yet";

	throw std::logic_error(e.str());
#else
	int status = ::kill(pid, sig);

	if (0 == status)
		return 0;

	if (EINVAL != errno)
		return errno;

	std::stringstream e;

	e << "Can't send invalid signal " << sig;

	throw std::logic_error(e.str());
#endif
}


int Signal::send(Signal::enum_t signame, const std::string & pid_file) {
	NutFile file(pid_file, NutFile::READ_ONLY);

	std::string pid_str;

	NutStream::status_t read_st = file.getString(pid_str);

	if (NutStream::NUTS_OK != read_st) {
		std::stringstream e;

		e << "Failed to read PID from " << pid_file << ": " << read_st;

		throw std::runtime_error(e.str());
	}

	std::stringstream pid_conv(pid_str);

	pid_t pid;

	if (!(pid_conv >> pid)) {
		std::stringstream e;

		e << "Failed to convert contents of " << pid_file << " to PID";

		throw std::runtime_error(e.str());
	}

	return send(signame, pid);
}


int NutSignal::send(NutSignal::enum_t signame, const std::string & process) {
	std::string pid_file;

	// TBD: What's ALTPIDPATH and shouldn't we also consider it?
	pid_file += PIDPATH;
	pid_file += '/';
	pid_file += process;
	pid_file += ".pid";

	return Signal::send(signame, pid_file);
}

}  // end of namespace nut
