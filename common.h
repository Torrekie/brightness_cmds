#ifndef __BRIGHTNESS_COMMON_H__
#define __BRIGHTNESS_COMMON_H__

#include <os/base.h>
#include <sys/types.h>
#include "switchs.h"

typedef enum {
	CMD_BACKLIGHT,	/* Display backlight */
	CMD_KBDLIGHT,	/* Keyboard backlight */
	CMD_FLASHLIGHT,	/* Flashlight */
} cmdopts;

typedef enum {
	SUBOPT_GET = 0x1,	/* Get value */
	SUBOPT_SET = 0x2,	/* Set value */
	SUBOPT_MAX = 0x4,	/* Maximum value */
	SUBOPT_MIN = 0x8,	/* Minimum value */
} cmdsubopts;

typedef enum {
	V_QUIET,		/* Do everything quietly */
	V_LOG,			/* Normal logging and warnings */
	V_VERBOSE,		/* More logging and warnings */
	V_DEBUG,		/* As much as possible */

	V_STDERR   = 0x10,	/* Output to stderr as formatted error message */
	V_WARN	 = 0x20,	/* Output to stderr as formatted warning message */
	V_DBGMSG   = 0x40,	/* Output to stderr as formatted debug message */
} cmdverbosity;


__BEGIN_DECLS
static char *argv0;
static cmdverbosity verbosity;

bool is_valid_value(const char *str, float *percent, float *value);
bool is_decimal_num(const char *str);
int verbose(int level, const char *fmt, ...);
void print_help(cmdopts opt);
__END_DECLS

#endif
