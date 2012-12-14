/* nutipc.hpp - NUT IPC

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

#ifndef NUT_NUTIPC_HPP
#define NUT_NUTIPC_HPP

#include <stdexcept>
#include <list>

extern "C" {
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
}


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
	static pid_t getPID() throw();

	/** Get parent process ID */
	static pid_t getPPID() throw();

	/**
	 *  Process main routine functor prototype
	 */
	class Main {
		protected:

		/** Formal constructor */
		Main() {}

		public:

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
		Child(M main) throw(std::runtime_error);

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
		int wait() throw(std::logic_error);

		/**
		 *  \brief  Child exit code getter
		 *
		 *  \return Child exit code
		 */
		inline int exitCode() {
			if (!m_exited) {
				m_exit_code = wait();

				m_exited = true;
			}

			return m_exit_code;
		}

		/**
		 *  \brief  Destructor
		 *
		 *  The destructor shall wait for the child process
		 *  (unless already exited).
		 */
		~Child() {
			if (!m_exited)
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

		const std::string m_bin;
		const Arguments   m_args;

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
		int operator () () throw(std::runtime_error);

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

};  // end of class Process


/**
 *  POSIX signal
 *
 *  For portability reasons, only mostly common subset of POSIX.1-2001 signals are supported.
 */
class Signal {
	public:

	/** Signals */
	typedef enum {
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
		USER2  = SIGUSR1,    /** User-defined signal 2             */
		CHILD  = SIGCHLD,    /** Child stopped or terminated       */
		CONT   = SIGCONT,    /** Continue if stopped               */
		STOP   = SIGSTOP,    /** Stop process (unmaskable)         */
		TSTOP  = SIGTSTP,    /** Stop typed at tty                 */
		TTYIN  = SIGTTIN,    /** tty input for background process  */
		TTYOUT = SIGTTOU,    /** tty output for background process */
		PROF   = SIGPROF,    /** Profiling timer expired           */
		SYS    = SIGSYS,     /** Bad argument to routine           */
		URG    = SIGURG,     /** Urgent condition on socket        */
		VTALRM = SIGVTALRM,  /** Virtual alarm clock               */
		XCPU   = SIGXCPU,    /** CPU time limit exceeded           */
		XFSZ   = SIGXFSZ,    /** File size limit exceeded          */
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
		virtual ~Handler() {}

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
			QUIT   = 0,  /**< Shutdown the thread */
			SIGNAL = 1,  /**< Signal obtained     */
		} command_t;

		/** Communication pipe */
		static int s_comm_pipe[2];

		/**
		 *  \brief  Signal handler thread main routine
		 *
		 *  The function synchronously read commands from the communication pipe.
		 *  It processes control commands on its own (e.g. the quit command).
		 *  It passes all signals to signal handler instance of \ref H
		 *  Which must implement the \ref Signal::Handler interface.
		 *  The handler is instantiated in scope of the routine.
		 *  It closes the communication pipe read end in reaction to \ref QUIT command.
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
		 *  thread communication pipe (as parameter of the \ref SIGNAL command).
		 *  The signal handling itself (whatever necessary) shall be done
		 *  by the dedicated thread (to avoid possible re-entrancy issues).
		 *
		 *  Note that \c ::write is required to be an async-signal-safe function by
		 *  POSIX.1-2004; also note that up to \c PIPE_BUF bytes are written atomicaly
		 *  as required by IEEE Std 1003.1, 2004 Edition,\c PIPE_BUF being typically
		 *  hundreds of bytes at least (POSIX requires 512B, Linux provides whole 4KiB
		 *  page).
		 *
		 *  \param  signal  Signal
		 */
		static void signalNotifier(int signal);

		/**
		 *  \brief  Constructor
		 *
		 *  The threads are only created by the (friend) Signal class
		 *  functions.
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
		HandlerThread(const Signal::List & siglist) throw(std::logic_error, std::runtime_error);

		public:

		/**
		 *  \brief  Terminate the thread
		 *
		 *  The method sends the signal handler thread the \ref QUIT signal.
		 *  It blocks until the thread is joined.
		 *  Closes the communication pipe write end.
		 */
		void quit() throw(std::runtime_error);

		/**
		 *  \brief  Destructor
		 *
		 *  Forces the signal handler thread termination (unless already down).
		 */
		~HandlerThread() throw(std::runtime_error);

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
	static int send(enum_t signame, pid_t pid) throw(std::logic_error);

};  // end of class Signal


/** Initialisation of the communication pipes */
template <class H>
int Signal::HandlerThread<H>::s_comm_pipe[2] = { -1, -1 };

}  // end of namespace nut

#endif /* end of #ifndef NUT_NUTIPC_H */
