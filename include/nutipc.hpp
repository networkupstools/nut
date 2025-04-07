/*
    nutipc.hpp - NUT IPC

    Copyright (C) 2012 Eaton

        Author: Vaclav Krpec  <VaclavKrpec@Eaton.com>

    Copyright (C) 2024-2025 NUT Community

        Author: Jim Klimov  <jimklimov+nut@gmail.com>

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

#ifndef NUT_NUTIPC_HPP
#define NUT_NUTIPC_HPP

#include <stdexcept>
#include <list>
#include <sstream>

#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cerrno>

extern "C" {
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#ifndef WIN32
# include <sys/wait.h>
#else	/* WIN32 */
# include "wincompat.h"
#endif	/* WIN32 */

#ifdef HAVE_PTHREAD
# include <pthread.h>
#endif
}

/* See include/common.h for details behind this */
#ifndef NUT_UNUSED_VARIABLE
#define NUT_UNUSED_VARIABLE(x) (void)(x)
#endif

namespace nut {

/**
 *  Process-related information
 */
class Process {
	private:

	/** The type yields no instances */
	Process() {}

	public:

	/** Get current process ID */
	static pid_t getPID()
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
		;

	/** Get parent process ID */
	static pid_t getPPID()
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
		;

	/**
	 *  Process main routine functor prototype
	 */
	class Main {
		protected:

		/** Formal constructor */
		Main() {}
		virtual ~Main();

		public:

		/* Avoid implicit copy/move operator declarations */
		Main(Main&&) = default;
		Main& operator=(const Main&) = default;
		//Main& operator=(Main&&) = default;

		/** Routine */
		virtual int operator () () = 0;

	};  // end of class Main

	/**
	 *  Child process
	 */
	template <class M>
	class Child {
		private:

		pid_t m_pid;        /**< Child PID    */
		bool  m_exited;     /**< Exited flag  */
		int   m_exit_code;  /**< Exit code    */

		public:

		/**
		 *  \brief  Constructor of child process
		 *
		 *  The constructor calls \c ::fork to create another child process.
		 *  The child executes \ref m_main functor instance operator \c ().
		 *  When the functor's \c () operator returns, the returned value
		 *  shall be used as the child exit code (and the child will exit).
		 *
		 *  \param  main  Child process main routine
		 */
		Child(M main)
#if (defined __cplusplus) && (__cplusplus < 201100)
			throw(std::runtime_error)
#endif
			;

		/** Child PID */
		inline pid_t getPID() const { return m_pid; }

		/**
		 *  \brief  Wait for child process to exit
		 *
		 *  The method blocks as long as the child runs.
		 *  It returns the child's exit code.
		 *  It throws an exception if executed twice
		 *  (or on other illogical usage).
		 *
		 *  \return Child process exit code
		 */
		int wait()
#if (defined __cplusplus) && (__cplusplus < 201100)
			throw(std::logic_error)
#endif
			;

		/**
		 *  \brief  Child exit code getter
		 *
		 *  \return Child exit code
		 */
		inline int exitCode() {
			return wait();
		}

		/**
		 *  \brief  Destructor
		 *
		 *  The destructor shall wait for the child process
		 *  (unless already exited).
		 */
		~Child() {
			wait();
		}

	};  // end of class Child

	/**
	 *  External command executor
	 */
	class Executor: public Main {
		public:

		/** Command line arguments list */
		typedef std::list<std::string> Arguments;

		private:

		std::string m_bin;
		Arguments   m_args;

		public:

		/**
		 *  \brief  Constructor
		 *
		 *  The binary path may be omitted; the implementation shall perform
		 *  the actions shell would do to search for the binary (i.e. check \c PATH
		 *  environment variable);
		 *
		 *  Note that even option switches are command line arguments;
		 *  e.g. "tail -n 20" command has 2 arguments: "-n" and "20".
		 *
		 *  \brief  bin   Binary to be executed
		 *  \brief  args  Command-line arguments to the binary
		 */
		Executor(const std::string & bin, const Arguments & args):
			m_bin(bin), m_args(args) {}

		/**
		 *  \brief  Constructor
		 *
		 *  This constructor form splits the command string specified
		 *  to the binary and its cmd-line arguments for the caller (by spaces).
		 *
		 *  Note however, that the command must be a binary execution; if you want
		 *  to run a shell command, you must execute the shell, explicitly; e.g:
		 *  "/bin/sh -c '<your shell command>'" shall probably be what you want.
		 *
		 *  \param  command  Command to be executed
		 */
		Executor(const std::string & command);

		/** Execution of the binary */
		int operator () ()
#if (defined __cplusplus) && (__cplusplus < 201100)
			throw(std::runtime_error)
#endif
			override;
	};  // end of class Executor

	/**
	 *  External command execution
	 */
	class Execution: public Child<Executor> {
		public:

		/**
		 *  Constructor
		 *
		 *  The binary path may be omitted; the implementation shall perform
		 *  the actions shell would do to search for the binary (i.e. check \c PATH
		 *  environment variable);
		 *
		 *  \brief  binary     Binary to be executed
		 *  \brief  arguments  Command-line arguments to the binary
		 */
		Execution(const std::string & binary, const Executor::Arguments & arguments):
			Child<Executor>(Executor(binary, arguments)) {}

		/**
		 *  Constructor
		 *
		 *  This form of the constructor splits the command string specified
		 *  to the binary and its cmd-line arguments for the caller (by spaces).
		 *
		 *  Note however, that the command must be a binary execution; if you want
		 *  to run a shell command, you must execute the shell, explicitly; e.g:
		 *  "/bin/sh -c '<your shell command>'" shall probably be what you want.
		 *
		 *  \param  command  Command to be executed
		 */
		Execution(const std::string & command): Child<Executor>(Executor(command)) {}

	};  // end of class Execution

	/**
	 *  \brief  Execute command and wait for exit code
	 *
	 *  \param  binary     Binary to be executed
	 *  \param  arguments  Command-line arguments to the binary
	 *
	 *  \return Exit code
	 */
	static inline int execute(const std::string & binary, const Executor::Arguments & arguments) {
		Execution child(binary, arguments);

		return child.wait();
	}

	/**
	 *  \brief  Execute command and wait for exit code
	 *
	 *  \param  command  Command to be executed
	 *
	 *  \return Exit code
	 */
	static inline int execute(const std::string & command) {
		Execution child(command);

		return child.wait();
	}

};  // end of class Process


template <class M>
Process::Child<M>::Child(M main)
#if (defined __cplusplus) && (__cplusplus < 201100)
	throw(std::runtime_error)
#endif
	:
	m_pid(0),
	m_exited(false),
	m_exit_code(0)
{
#ifdef WIN32
	NUT_UNUSED_VARIABLE(main);

	/* FIXME: Implement (for NUT processes) via pipes?
	 * See e.g. upsdrvctl implementation. */
	std::stringstream e;

	e << "Can't fork: not implemented on this platform yet";

	/* NUT_WIN32_INCOMPLETE(); */
	throw std::logic_error(e.str());
#else	/* !WIN32 */
	m_pid = ::fork();

	if (!m_pid)
		::exit(main());
#endif	/* !WIN32 */
}


template <class M>
int Process::Child<M>::wait()
#if (defined __cplusplus) && (__cplusplus < 201100)
	throw(std::logic_error)
#endif
{
#ifdef WIN32
	/* FIXME: Implement (for NUT processes) via pipes?
	 * See e.g. upsdrvctl implementation. */
	std::stringstream e;

	e << "Can't wait for PID " << m_pid <<
		": not implemented on this platform yet";

	/* NUT_WIN32_INCOMPLETE(); */
	throw std::logic_error(e.str());
#else	/* !WIN32 */
	if (m_exited)
		return m_exit_code;

	pid_t wpid = ::waitpid(m_pid, &m_exit_code, 0);

	if (-1 == wpid) {
		int erno = errno;

		std::stringstream e;

		e << "Failed to wait for process " << m_pid << ": ";
		e << erno << ": " << strerror(erno);

		throw std::logic_error(e.str());
	}

	m_exited    = true;
	m_exit_code = WEXITSTATUS(m_exit_code);

	return m_exit_code;
#endif	/* !WIN32 */
}


/**
 *  POSIX signal
 *
 *  For portability reasons, only mostly common subset of POSIX.1-2001 signals are supported.
 */
class Signal {
	public:

	/** Signals */
	typedef enum {
#ifndef WIN32
		HUP    = SIGHUP,     /** Hangup                            */
		INT    = SIGINT,     /** Interrupt                         */
		QUIT   = SIGQUIT,    /** Quit                              */
		ILL    = SIGILL,     /** Illegal Instruction               */
		TRAP   = SIGTRAP,    /** Trace/breakpoint trap             */
		ABORT  = SIGABRT,    /** Abort                             */
		BUS    = SIGBUS,     /** Bus error (bad memory access)     */
		FPE    = SIGFPE,     /** Floating point exception          */
		KILL   = SIGKILL,    /** Kill (unmaskable)                 */
		SEGV   = SIGSEGV,    /** Invalid memory reference          */
		PIPE   = SIGPIPE,    /** Broken pipe                       */
		ALARM  = SIGALRM,    /** Alarm                             */
		TERM   = SIGTERM,    /** Termination                       */
		USER1  = SIGUSR1,    /** User-defined signal 1             */
		USER2  = SIGUSR2,    /** User-defined signal 2             */
		CHILD  = SIGCHLD,    /** Child stopped or terminated       */
		CONT   = SIGCONT,    /** Continue if stopped               */
		STOP   = SIGSTOP,    /** Stop process (unmaskable)         */
		TSTOP  = SIGTSTP,    /** Stop typed at TTY                 */
		TTYIN  = SIGTTIN,    /** TTY input for background process  */
		TTYOUT = SIGTTOU,    /** TTY output for background process */
		PROF   = SIGPROF,    /** Profiling timer expired           */
		SYS    = SIGSYS,     /** Bad argument to routine           */
		URG    = SIGURG,     /** Urgent condition on socket        */
		VTALRM = SIGVTALRM,  /** Virtual alarm clock               */
		XCPU   = SIGXCPU,    /** CPU time limit exceeded           */
		XFSZ   = SIGXFSZ,    /** File size limit exceeded          */
#endif	/* !WIN32 */
	} enum_t;  // end of typedef enum

	/** Signal list */
	typedef std::list<enum_t> List;

	/**
	 *  \brief  Signal handler
	 *
	 *  Signal handler interface.
	 */
	class Handler {
		protected:

		/** Formal constructor */
		Handler() {}

		public:

		/**
		 *  \brief  Signal handler routine
		 *
		 *  \param  signal  Signal
		 */
		virtual void operator () (enum_t signal) = 0;

		/** Formal destructor */
		virtual ~Handler();

	};  // end of class Handler

	private:

	/** Formal constructor */
	Signal() {}

	public:

	/** Signal handler thread handle */
	template <class H>
	class HandlerThread {
		friend class Signal;

		private:

		/** Control commands */
		typedef enum {
			HT_QUIT   = 0,  /**< Shutdown the thread */
			HT_SIGNAL = 1,  /**< Signal obtained     */
		} command_t;

		/** Communication pipe */
		static int s_comm_pipe[2];

		/** POSIX thread */
		pthread_t m_impl;

		/**
		 *  \brief  Signal handler thread main routine
		 *
		 *  The function synchronously read commands from the communication pipe.
		 *  It processes control commands on its own (e.g. the quit command).
		 *  It passes all signals to signal handler instance of \ref H
		 *  Which must implement the \ref Signal::Handler interface.
		 *  The handler is instantiated in scope of the routine.
		 *  It closes the communication pipe read end in reaction to
		 *  \ref HT_QUIT command.
		 *
		 *  \param  comm_pipe_read_end  Communication pipe read end
		 *
		 *  \retval N/A (the function never returns)
		 */
		static void * main(void * comm_pipe_read_end);

		/**
		 *  \brief  Signal handler routine
		 *
		 *  The actual signal handler routine executed by the OS when the process
		 *  obtains signal to be handled.
		 *  The function simply writes the signal number to the signal handler
		 *  thread communication pipe (as parameter of the \ref HT_SIGNAL command).
		 *  The signal handling itself (whatever necessary) shall be done
		 *  by the dedicated thread (to avoid possible re-entrance issues).
		 *
		 *  Note that \c ::write is required to be an async-signal-safe function by
		 *  POSIX.1-2004; also note that up to \c PIPE_BUF bytes are written atomically
		 *  as required by IEEE Std 1003.1, 2004 Edition,\c PIPE_BUF being typically
		 *  hundreds of bytes at least (POSIX requires 512B, Linux provides whole 4KiB
		 *  page).
		 *
		 *  \param  signal  Signal
		 */
		static void signalNotifier(int signal);

		public:

		/**
		 *  \brief  Constructor
		 *
		 *  At most one thread per handler instance may be created.
		 *  This limitation is both due sanity reasons (it wouldn't
		 *  make much sense to handle the same signal by multiple threads)
		 *  and because of the signal handler routine only has access
		 *  to one communication pipe write end (the static member).
		 *  This is actually the only technical reason of having the class
		 *  template (so that every instance has its own static comm. queue).
		 *  However, for different handler classes, multiple handling threads
		 *  may be created (if it makes sense) since these will be different
		 *  template instances and therefore will use different static
		 *  communication pipes.
		 *  If more than 1 instance creation is attempted, an exception is thrown.
		 *
		 *  \param  siglist  List of signals that shall be handled by the thread
		 */
		HandlerThread(const Signal::List & siglist)
#if (defined __cplusplus) && (__cplusplus < 201100)
			throw(std::logic_error, std::runtime_error)
#endif
			;

		/**
		 *  \brief  Terminate the thread
		 *
		 *  The method sends the signal handler thread the \ref QUIT signal.
		 *  It blocks until the thread is joined.
		 *  Closes the communication pipe write end.
		 */
		void quit()
#if (defined __cplusplus) && (__cplusplus < 201100)
			throw(std::runtime_error)
#endif
			;

		/**
		 *  \brief  Destructor
		 *
		 *  Forces the signal handler thread termination (unless already down).
		 */
		~HandlerThread()
#if (defined __cplusplus) && (__cplusplus < 201100)
			throw(std::runtime_error)
#endif
			;

	};  // end of HandlerThread

	/**
	 *  \brief  Send signal to a process
	 *
	 *  An exception is thrown if the signal isn't implemented.
	 *
	 *  \param  signame  Signal name
	 *  \param  pid      Process ID
	 *
	 *  \retval 0     in case of success
	 *  \retval EPERM if the process doesn't have permission to send the signal
	 *  \retval ESRCH if the process (group) identified doesn't exist
	 */
	static int send(enum_t signame, pid_t pid)
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw(std::logic_error)
#endif
		;

	/**
	 *  \brief  Send signal to a process identified via PID file
	 *
	 *  An exception is thrown if the signal isn't implemented
	 *  or PID file read fails.
	 *
	 *  \param  signame   Signal name
	 *  \param  pid_file  File containing process PID
	 *
	 *  \retval 0     in case of success
	 *  \retval EPERM if the process doesn't have permission to send the signal
	 *  \retval ESRCH if the process (group) identified doesn't exist
	 */
	static int send(enum_t signame, const std::string & pid_file);

};  // end of class Signal


/** Initialization of the communication pipes */
template <class H>
int Signal::HandlerThread<H>::s_comm_pipe[2] = { -1, -1 };


template <class H>
void * Signal::HandlerThread<H>::main(void * comm_pipe_read_end) {
	int rfd = *(reinterpret_cast<int *>(comm_pipe_read_end));

	H handler;

	for (;;) {
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(rfd, &rfds);

		// Poll on signal pipe
		// Note that a straightforward blocking read could be
		// also used.  However, select allows specifying a
		// timeout, which could be useful in the future (but
		// is not used in the current code).
		int fdno = ::select(FD_SETSIZE, &rfds, nullptr, nullptr, nullptr);

		// -1 is an error, but EINTR means the system call was
		// interrupted.  System calls are expected to be
		// interrupted on signal delivery; some systems
		// restart them, and some don't.  Getting EINTR is
		// therefore not actually an error, and the standard
		// approach is to retry.
		if (-1 == fdno) {
			if (errno == EINTR)
				continue;

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

		// POSIX probably does not prohibit reading some but
		// not all of the multibyte message.  However, it is
		// unlikely that an implementation using write and
		// read on int-sized or smaller objects will split
		// them.  For now our strategy is to hope this does
		// not happen.
		assert(sizeof(word) == read_out);

		command_t command = static_cast<command_t>(word);

		switch (command) {
			case HT_QUIT:
				// Close comm. pipe read end
				::close(rfd);

				// Terminate thread
				::pthread_exit(nullptr);

			case HT_SIGNAL:
				{	// scoping
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

				Signal::enum_t sig = static_cast<Signal::enum_t>(word);

				// Handle signal
				handler(sig);
				}	// scoping
				break;

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
			/* Must not occur thanks to enum.
			 * But otherwise we can see
			 *   error: 'switch' missing 'default' label [-Werror,-Wswitch-default]
			 * from some overly zealous compilers.
			 */
			default:
				throw std::logic_error("INTERNAL ERROR: Unexpected default case reached");
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
		}
	}

	// Pro-forma exception
	throw std::logic_error("INTERNAL ERROR: Unreachable code reached");
}


/**
 *  \brief  Write command to command pipe
 *
 *  \param  fh        Pipe writing end (file handle)
 *  \param  cmd       Command
 *  \param  cmd_size  Command size
 *
 *  \retval 0     on success
 *  \retval errno on error
 */
int sigPipeWriteCmd(int fh, void * cmd, size_t cmd_size)
#if (defined __cplusplus) && (__cplusplus < 201100)
	throw(std::runtime_error)
#endif
	;


template <class H>
void Signal::HandlerThread<H>::signalNotifier(int signal) {
	int sig[2] = {
		static_cast<int>(Signal::HandlerThread<H>::HT_SIGNAL),
	};

	sig[1] = signal;

	// TBD: The return value is silently ignored.
	// Either the write should've succeeded or the handling
	// thread is already coming down...
	sigPipeWriteCmd(s_comm_pipe[1], sig, sizeof(sig));
}


template <class H>
Signal::HandlerThread<H>::HandlerThread(const Signal::List & siglist)
#if (defined __cplusplus) && (__cplusplus < 201100)
	throw(std::logic_error, std::runtime_error)
#endif
{
#ifdef WIN32
	NUT_UNUSED_VARIABLE(siglist);

	/* FIXME: Implement (for NUT processes) via pipes?
	 * See e.g. upsdrvctl implementation. */
	std::stringstream e;

	e << "Can't prepare signal handling thread: not implemented on this platform yet";

	/* NUT_WIN32_INCOMPLETE(); */
	throw std::logic_error(e.str());
#else	/* !WIN32 */
	// At most one instance per process allowed
	if (-1 != s_comm_pipe[1])
		throw std::logic_error(
			"Attempt to start a duplicate of signal handling thread detected");

	// Create communication pipe
	if (::pipe(s_comm_pipe)) {
		std::stringstream e;

		e << "Failed to create communication pipe: " << errno;

		throw std::runtime_error(e.str());
	}

	// Start the thread
	int status = ::pthread_create(&m_impl, nullptr, &main, s_comm_pipe);

	if (status) {
		std::stringstream e;

		e << "Failed to start the thread: " << status;

		throw std::runtime_error(e.str());
	}

	// Register signals
	Signal::List::const_iterator sig = siglist.begin();

	for (; sig != siglist.end(); ++sig) {
		struct sigaction action;

		::memset(&action, 0, sizeof(action));
# ifdef sigemptyset
		// no :: here because macro
		sigemptyset(&action.sa_mask);
# else
		::sigemptyset(&action.sa_mask);
# endif

		action.sa_handler = &signalNotifier;

		int signo = static_cast<int>(*sig);

		// TBD: We might want to save the old handlers...
		status = ::sigaction(signo, &action, nullptr);

		if (status) {
			std::stringstream e;

			e << "Failed to register signal handler for signal ";
			e << signo << ": " << errno;

			throw std::runtime_error(e.str());
		}
	}
#endif	/* !WIN32 */
}


template <class H>
void Signal::HandlerThread<H>::quit()
#if (defined __cplusplus) && (__cplusplus < 201100)
	throw(std::runtime_error)
#endif
{
	static int quit = static_cast<int>(Signal::HandlerThread<H>::HT_QUIT);

	sigPipeWriteCmd(s_comm_pipe[1], &quit, sizeof(quit));

	int status = ::pthread_join(m_impl, nullptr);

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
}


template <class H>
Signal::HandlerThread<H>::~HandlerThread
#if (defined __clang__)
	<H>
#endif
()
#if (defined __cplusplus) && (__cplusplus < 201100)
	throw(std::runtime_error)
#endif
{
	// Stop the thread unless already stopped
	if (-1 != s_comm_pipe[1])
		quit();
}


/** NUT-specific signal handling */
class NutSignal: public Signal {
	public:

	/**
	 *  \brief  Send signal to a NUT process
	 *
	 *  The function assembles process-specific PID file name and path
	 *  and calls \ref Signal::send.
	 *
	 *  An exception is thrown if the signal isn't implemented
	 *  or PID file read fails.
	 *
	 *  \param  signame  Signal name
	 *  \param  process  File containing process PID
	 *
	 *  \retval 0     in case of success
	 *  \retval EPERM if the process doesn't have permission to send the signal
	 *  \retval ESRCH if the process (group) identified doesn't exist
	 */
	static int send(enum_t signame, const std::string & process);

};  // end of class NutSignal

}  // end of namespace nut

#endif /* end of #ifndef NUT_NUTIPC_H */
