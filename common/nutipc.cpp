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

#include <sstream>

#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cerrno>

extern "C" {
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
}


namespace nut {

pid_t Process::getPID() throw() {
	return getpid();
}


pid_t Process::getPPID() throw() {
	return getppid();
}


template <class M>
Process::Child<M>::Child(M main) throw(std::runtime_error):
	m_pid(0),
	m_exited(false),
	m_exit_code(0)
{
	m_pid = ::fork();

	if (!m_pid)
		::exit(main());
}


template <class M>
int Process::Child<M>::wait() throw(std::logic_error) {
	int exit_code;
	pid_t wpid = ::waitpid(m_pid, &exit_code, 0);

	if (-1 == m_pid) {
		int erno = errno;

		std::stringstream e;

		e << "Failed to wait for process " << m_pid << ": ";
		e << erno << ": " << strerror(erno);

		throw std::logic_error(e.str());
	}

	return exit_code;
}


Process::Executor::Executor(const std::string & command) {
	//m_bin m_args
	throw std::runtime_error("TODO: Not implemented, yet");
}


int Process::Executor::operator () () throw(std::runtime_error) {
        const char ** args_c_str = new const char *[m_args.size()];

	Arguments::const_iterator arg = m_args.begin();

	for (size_t i = 0; arg != m_args.end(); ++arg, ++i) {
		args_c_str[i] = (*arg).c_str();
	}

	int status = ::execvp(m_bin.c_str(), (char * const *)args_c_str);

	// Upon successful execution, the execvp function never returns
	// (since the process context is replaced, completely)

	delete args_c_str;

	std::stringstream e;

	e << "Failed to execute binary " << m_bin << ": " << status;

	throw std::runtime_error(e.str());
}


template <class H>
void * Signal::HandlerThread<H>::main(void * comm_pipe_read_end) {
	int rfd = *(int *)comm_pipe_read_end;

	H handler;

	for (;;) {
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(rfd, &rfds);

		// Poll on signal pipe
		// Note that direct blocking read could be also used;
		// however, select allows timeout specification
		// which might come handy...
		int fdno = ::select(1, &rfds, NULL, NULL, NULL);

		// TBD: Die or recover on error?
		if (-1 == fdno) {
			std::stringstream e;

			e << "Poll on communication pipe read end ";
			e << rfd << " failed: " << errno;

			throw std::runtime_error(e.str());
		}

		assert(1 == fdno);
		assert(FD_ISSET(rfd, &rfds));

		// Read command
		int word;

		ssize_t read_out = ::read(rfd, &word, sizeof(word));

		// TBD: again, how should we treat read error?
		if (-1 == read_out) {
			std::stringstream e;

			e << "Failed to read command from the command pipe: ";
			e << errno;

			throw std::runtime_error(e.str());
		}

		assert(sizeof(word) == read_out);

		command_t command = reinterpret_cast<command_t>(word);

		switch (command) {
			case QUIT:
				pthread_exit(NULL);

			case SIGNAL:
				// Read signal number
				read_out = ::read(rfd, &word, sizeof(word));

				// TBD: again, how should we treat read error?
				if (-1 == read_out) {
					std::stringstream e;

					e << "Failed to read signal number ";
					e << "from the command pipe: " << errno;

					throw std::runtime_error(e.str());
				}

				assert(sizeof(word) == read_out);

				Signal::enum_t sig = reinterpret_cast<Signal::enum_t>(word);

				// Handle signal
				handler(sig);
		}
	}

	// Pro-forma exception
	throw std::logic_error("INTERNAL ERROR: Unreachable code reached");
}


/**
 *  \brief  Write command to command pipe
 *
 *  \param  fh        Pipe writing end
 *  \param  cmd       Command
 *  \param  cmd_size  Comand size
 *
 *  \retval 0     on success
 *  \retval errno on error
 */
static int writeCommand(int fh, void * cmd, size_t cmd_size) throw(std::runtime_error) {
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


template <class H>
void Signal::HandlerThread<H>::signalNotifier(int signal) {
	int sig[2] = {
		reinterpret_cast<int>(Signal::HandlerThread<H>::SIGNAL),
	};

	sig[1] = signal;

	// TBD: The return value is silently ignored.
	// Either the write should've succeeded or the handling
	// thread is already comming down...
	writeCommand(s_comm_pipe[1], sig, sizeof(sig));
}


template <class H>
Signal::HandlerThread<H>::HandlerThread(const Signal::List & siglist) throw(std::logic_error, std::runtime_error) {
	/*
	 *  IMPLEMENTATION NOTES:
	 *  0/ Check whether comm. pipe is valid; throw std::logic_error if so
	 *  1/ Comm. pipe creation (or an exception)
	 *  2/ Start the thread: pthread_create(m_impl, <a static function>, <handlers copy (!)>)
	 *  3/ Register the signals: sigaction( <signal handler using write to wake & inform the thread about a signal> )
	 */
	throw std::runtime_error("TODO: Signal handler thread is not implemented, yet");

	if (-1 != s_comm_pipe[1])
		throw std::logic_error("Attempt to start a duplicate of signal handling thread detected");

	// Create communication pipe
	if (::pipe(s_comm_pipe)) {
		std::stringstream e;

		e << "Failed to create communication pipe: " << errno;

		throw std::runtime_error(e.str());
	}

	// TODO:
#if (0)
	// Start the thread
	int status = ::pthread_create(&m_impl, NULL, main, s_comm_pipe);

	if (status) {
		std::stringstream e;

		e << "Failed to start the thread: " << status;

		throw std::runtime_error(e.str());
	}

	// Register signals
	Signal::List::const_iterator sig = siglist.begin();

	for (; sig != siglist.end(); ++sig) {
		struct sigaction action;

		action.sa_handler   = signalNotifier;
		action.sa_sigaction = NULL;  // guess we don't need signal info
		action.sa_mask      = 0;     // shouldn't we mask signals while handling them?
		action.sa_flags     = 0;     // any flags?
		action.sa_restorer  = NULL;  // obsolete

		int signo = static_cast<int>(*sig);

		// TBD: We might want to save the old handlers...
		status = ::sigaction(signo, &action, NULL);

		if (status) {
			std::stringstream e;

			e << "Failed to register signal handler for signal ";
			e << signo << ": " << errno;

			throw std::runtime_error(e.str());
		}
	}
#endif
}


template <class H>
void Signal::HandlerThread<H>::quit() throw(std::runtime_error) {
	static int quit = reinterpret_cast<int>(Signal::HandlerThread<H>::QUIT);

	writeCommand(s_comm_pipe[1], quit, sizeof(quit));

	// TODO:
#if (0)
	int status = ::pthread_join(m_impl);

	if (status) {
		std::stringstream e;

		e << "Failed to joint signal handling thread: " << status;

		throw std::runtime_error(e.str());
	}

	if (::close(s_comm_pipe[1])) {
		std::stringstream e;

		e << "Failed to close communication pipe: " << errno;

		throw std::runtime_error(e.str());
	}

	s_comm_pipe[1] = -1;
#endif
}


template <class H>
Signal::HandlerThread<H>::~HandlerThread() throw(std::runtime_error) {
	// Stop the thread unless already stopped
	if (-1 != s_comm_pipe[1])
		quit();
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
