/* upssched.h - supporting structures */

#ifndef NUT_UPSSCHED_H_SEEN
#define NUT_UPSSCHED_H_SEEN 1

#include <parseconf.h>

#define SERIALIZE_INIT 1
#define SERIALIZE_SET  2
#define SERIALIZE_WAIT 3

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* track client connections */
typedef struct conn_s {
	int     fd;
	PCONF_CTX_t	ctx;
	struct conn_s	*next;
} conn_t;

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif	/* NUT_UPSSCHED_H_SEEN */
