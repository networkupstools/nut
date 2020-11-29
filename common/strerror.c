/* strerror()  Mark Powell <medp@primagraphics.co.uk> */
/* Simple implementation derived from libiberty */

#include "config.h"

#ifndef HAVE_STRERROR

#include <errno.h>

char *strerror(int errnum)
{
    static char buf[32];

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE_BREAK
#pragma GCC diagnostic ignored "-Wunreachable-code-break"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic ignored "-Wunreachable-code"
#endif

    switch (errnum) {
#if defined (EPERM)
      case EPERM:
        return "Not owner";
#endif
#if defined (ENOENT)
      case ENOENT:
        return "No such file or directory";
#endif
#if defined (ESRCH)
      case ESRCH:
        return "No such process";
#endif
#if defined (EINTR)
      case EINTR:
        return "Interrupted system call";
#endif
#if defined (EIO)
      case EIO:
        return "I/O error";
#endif
#if defined (ENXIO)
      case ENXIO:
        return "No such device or address";
#endif
#if defined (E2BIG)
        return "Arg list too long";
#endif
#if defined (ENOEXEC)
      case ENOEXEC:
        return "Exec format error";
#endif
#if defined (EBADF)
      case EBADF:
        return "Bad file number";
#endif
#if defined (ECHILD)
      case ECHILD:
        return "No child processes";
#endif
#if defined (EWOULDBLOCK)	/* Put before EAGAIN, sometimes aliased */
      case EWOULDBLOCK:
        return "Operation would block";
#endif
#if defined (EAGAIN)
#if defined (EWOULDBLOCK) && EAGAIN != EWOULDBLOCK
      case EAGAIN:
        return "No more processes";
#endif
#endif
#if defined (ENOMEM)
      case ENOMEM:
        return "Not enough space";
#endif
#if defined (EACCES)
      case EACCES:
        return "Permission denied";
#endif
#if defined (EFAULT)
      case EFAULT:
        return "Bad address";
#endif
#if defined (ENOTBLK)
      case ENOTBLK:
        return "Block device required";
#endif
#if defined (EBUSY)
      case EBUSY:
        return "Device busy";
#endif
#if defined (EEXIST)
      case EEXIST:
        return "File exists";
#endif
#if defined (EXDEV)
      case EXDEV:
        return "Cross-device link";
#endif
#if defined (ENODEV)
      case ENODEV:
        return "No such device";
#endif
#if defined (ENOTDIR)
      case ENOTDIR:
        return "Not a directory";
#endif
#if defined (EISDIR)
      case EISDIR:
        return "Is a directory";
#endif
#if defined (EINVAL)
      case EINVAL:
        return "Invalid argument";
#endif
#if defined (ENFILE)
      case ENFILE:
        return "File table overflow";
#endif
#if defined (EMFILE)
      case EMFILE:
        return "Too many open files";
#endif
#if defined (ENOTTY)
      case ENOTTY:
        return "Not a typewriter";
#endif
#if defined (ETXTBSY)
      case ETXTBSY:
        return "Text file busy";
#endif
#if defined (EFBIG)
      case EFBIG:
        return "File too large";
#endif
#if defined (ENOSPC)
      case ENOSPC:
        return "No space left on device";
#endif
#if defined (ESPIPE)
      case ESPIPE:
        return "Illegal seek";
#endif
#if defined (EROFS)
      case EROFS:
        return "Read-only file system";
#endif
#if defined (EMLINK)
      case EMLINK:
        return "Too many links";
#endif
#if defined (EPIPE)
      case EPIPE:
        return "Broken pipe";
#endif
#if defined (EDOM)
      case EDOM:
        return "Math argument out of domain of func";
#endif
#if defined (ERANGE)
      case ERANGE:
        return "Math result not representable";
#endif
#if defined (ENOMSG)
      case ENOMSG:
        return "No message of desired type";
#endif
#if defined (EIDRM)
      case EIDRM:
        return "Identifier removed";
#endif
#if defined (ECHRNG)
      case ECHRNG:
        return "Channel number out of range";
#endif
#if defined (EL2NSYNC)
        return "Level 2 not synchronized";
#endif
#if defined (EL3HLT)
        return "Level 3 halted";
#endif
#if defined (EL3RST)
        return "Level 3 reset";
#endif
#if defined (ELNRNG)
      case ELNRNG:
        return "Link number out of range";
#endif
#if defined (EUNATCH)
      case EUNATCH:
        return "Protocol driver not attached";
#endif
#if defined (ENOCSI)
      case ENOCSI:
        return "No CSI structure available";
#endif
#if defined (EL2HLT)
        return "Level 2 halted";
#endif
#if defined (EDEADLK)
      case EDEADLK:
        return "Deadlock condition";
#endif
#if defined (ENOLCK)
      case ENOLCK:
        return "No record locks available";
#endif
#if defined (EBADE)
      case EBADE:
        return "Invalid exchange";
#endif
#if defined (EBADR)
      case EBADR:
        return "Invalid request descriptor";
#endif
#if defined (EXFULL)
      case EXFULL:
        return "Exchange full";
#endif
#if defined (ENOANO)
      case ENOANO:
        return "No anode";
#endif
#if defined (EBADRQC)
      case EBADRQC:
        return "Invalid request code";
#endif
#if defined (EBADSLT)
      case EBADSLT:
        return "Invalid slot";
#endif
#if defined (EDEADLOCK)
      case EDEADLOCK:
        return "File locking deadlock error";
#endif
#if defined (EBFONT)
      case EBFONT:
        return "Bad font file format";
#endif
#if defined (ENOSTR)
      case ENOSTR:
        return "Device not a stream";
#endif
#if defined (ENODATA)
      case ENODATA:
        return "No data available";
#endif
#if defined (ETIME)
      case ETIME:
        return "Timer expired";
#endif
#if defined (ENOSR)
      case ENOSR:
        return "Out of streams resources";
#endif
#if defined (ENONET)
      case ENONET:
        return "Machine is not on the network";
#endif
#if defined (ENOPKG)
      case ENOPKG:
        return "Package not installed";
#endif
#if defined (EREMOTE)
      case EREMOTE:
        return "Object is remote";
#endif
#if defined (ENOLINK)
      case ENOLINK:
        return "Link has been severed";
#endif
#if defined (EADV)
      case EADV:
        return "Advertise error";
#endif
#if defined (ESRMNT)
      case ESRMNT:
        return "Srmount error";
#endif
#if defined (ECOMM)
      case ECOMM:
        return "Communication error on send";
#endif
#if defined (EPROTO)
      case EPROTO:
        return "Protocol error";
#endif
#if defined (EMULTIHOP)
      case EMULTIHOP:
        return "Multihop attempted";
#endif
#if defined (EDOTDOT)
      case EDOTDOT:
        return "RFS specific error";
#endif
#if defined (EBADMSG)
      case EBADMSG:
        return "Not a data message";
#endif
#if defined (ENAMETOOLONG)
      case ENAMETOOLONG:
        return "File name too long";
#endif
#if defined (EOVERFLOW)
      case EOVERFLOW:
        return "Value too large for defined data type";
#endif
#if defined (ENOTUNIQ)
      case ENOTUNIQ:
        return "Name not unique on network";
#endif
#if defined (EBADFD)
      case EBADFD:
        return "File descriptor in bad state";
#endif
#if defined (EREMCHG)
      case EREMCHG:
        return "Remote address changed";
#endif
#if defined (ELIBACC)
      case ELIBACC:
        return "Can not access a needed shared library";
#endif
#if defined (ELIBBAD)
      case ELIBBAD:
        return "Accessing a corrupted shared library";
#endif
#if defined (ELIBSCN)
      case ELIBSCN:
        return ".lib section in a.out corrupted";
#endif
#if defined (ELIBMAX)
      case ELIBMAX:
        return "Attempting to link in too many shared libraries";
#endif
#if defined (ELIBEXEC)
      case ELIBEXEC:
        return "Cannot exec a shared library directly";
#endif
#if defined (EILSEQ)
      case EILSEQ:
        return "Illegal byte sequence";
#endif
#if defined (ENOSYS)
      case ENOSYS:
        return "Operation not applicable";
#endif
#if defined (ELOOP)
      case ELOOP:
        return "Too many symbolic links encountered";
#endif
#if defined (ERESTART)
      case ERESTART:
        return "Interrupted system call should be restarted";
#endif
#if defined (ESTRPIPE)
      case ESTRPIPE:
        return "Streams pipe error";
#endif
#if defined (ENOTEMPTY)
      case ENOTEMPTY:
        return "Directory not empty";
#endif
#if defined (EUSERS)
      case EUSERS:
        return "Too many users";
#endif
#if defined (ENOTSOCK)
      case ENOTSOCK:
        return "Socket operation on non-socket";
#endif
#if defined (EDESTADDRREQ)
      case EDESTADDRREQ:
        return "Destination address required";
#endif
#if defined (EMSGSIZE)
      case EMSGSIZE:
        return "Message too long";
#endif
#if defined (EPROTOTYPE)
      case EPROTOTYPE:
        return "Protocol wrong type for socket";
#endif
#if defined (ENOPROTOOPT)
      case ENOPROTOOPT:
        return "Protocol not available";
#endif
#if defined (EPROTONOSUPPORT)
      case EPROTONOSUPPORT:
        return "Protocol not supported";
#endif
#if defined (ESOCKTNOSUPPORT)
      case ESOCKTNOSUPPORT:
        return "Socket type not supported";
#endif
#if defined (EOPNOTSUPP)
      case EOPNOTSUPP:
        return "Operation not supported on transport endpoint";
#endif
#if defined (EPFNOSUPPORT)
      case EPFNOSUPPORT:
        return "Protocol family not supported";
#endif
#if defined (EAFNOSUPPORT)
      case EAFNOSUPPORT:
        return "Address family not supported by protocol";
#endif
#if defined (EADDRINUSE)
      case EADDRINUSE:
        return "Address already in use";
#endif
#if defined (EADDRNOTAVAIL)
      case EADDRNOTAVAIL:
        return "Cannot assign requested address";
#endif
#if defined (ENETDOWN)
      case ENETDOWN:
        return "Network is down";
#endif
#if defined (ENETUNREACH)
      case ENETUNREACH:
        return "Network is unreachable";
#endif
#if defined (ENETRESET)
      case ENETRESET:
        return "Network dropped connection because of reset";
#endif
#if defined (ECONNABORTED)
      case ECONNABORTED:
        return "Software caused connection abort";
#endif
#if defined (ECONNRESET)
      case ECONNRESET:
        return "Connection reset by peer";
#endif
#if defined (ENOBUFS)
      case ENOBUFS:
        return "No buffer space available";
#endif
#if defined (EISCONN)
      case EISCONN:
        return "Transport endpoint is already connected";
#endif
#if defined (ENOTCONN)
      case ENOTCONN:
        return "Transport endpoint is not connected";
#endif
#if defined (ESHUTDOWN)
      case ESHUTDOWN:
        return "Cannot send after transport endpoint shutdown";
#endif
#if defined (ETOOMANYREFS)
      case ETOOMANYREFS:
        return "Too many references: cannot splice";
#endif
#if defined (ETIMEDOUT)
      case ETIMEDOUT:
        return "Connection timed out";
#endif
#if defined (ECONNREFUSED)
      case ECONNREFUSED:
        return "Connection refused";
#endif
#if defined (EHOSTDOWN)
      case EHOSTDOWN:
        return "Host is down";
#endif
#if defined (EHOSTUNREACH)
      case EHOSTUNREACH:
        return "No route to host";
#endif
#if defined (EALREADY)
      case EALREADY:
        return "Operation already in progress";
#endif
#if defined (EINPROGRESS)
      case EINPROGRESS:
        return "Operation now in progress";
#endif
#if defined (ESTALE)
      case ESTALE:
        return "Stale NFS file handle";
#endif
#if defined (EUCLEAN)
      case EUCLEAN:
        return "Structure needs cleaning";
#endif
#if defined (ENOTNAM)
      case ENOTNAM:
        return "Not a XENIX named type file";
#endif
#if defined (ENAVAIL)
      case ENAVAIL:
        return "No XENIX semaphores available";
#endif
#if defined (EISNAM)
      case EISNAM:
        return "Is a named type file";
#endif
#if defined (EREMOTEIO)
      case EREMOTEIO:
        return "Remote I/O error";
#endif
    }

    /* Fallback: just print the error number */
    snprintf(buf, sizeof(buf), "Error %d", errnum);
    return buf;

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic pop
#endif

}

#endif /* HAVE_STRERROR */
