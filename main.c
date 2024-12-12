#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dlfcn.h>

#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

#include "common.h"

typedef struct __IOMobileFramebuffer *IOMobileFramebuffer;

extern char *__progname;

/* backlight.m */
extern bool backlight_dcp(void);
extern bool backlight_ctrl(cmdsubopts opt, unsigned long *raw, const char *__nullable display);

/* We match [+|-][float][%] values */
bool is_valid_value(const char *str, float *percent, float *value)
{
	if (!str || *str == '\0') return false; /* null or empty string */

	const char *ptr = str;
	bool has_sign = false;
	bool is_percentage = false;

	/* optional leading sign */
	if (*ptr == '+' || *ptr == '-') {
		has_sign = true;
		ptr++;
	}

	bool has_digits = false;
	bool has_point = false;

	/* if the string is a valid number or percentage */
	while (*ptr) {
		if (isdigit(*ptr)) {
			has_digits = true;
		} else if (*ptr == '.') {
			if (has_point) {
				return false; /* more than one point */
			}
			has_point = true;
		} else if (*ptr == '%') {
			if (*(ptr + 1) != '\0') {
				return false; /* '%' must be at the end */
			}
			is_percentage = true;
			break;
		} else {
			return false; /* invalid char */
		}
		ptr++;
	}

	/* If there's no digits, it's not a valid number */
	if (!has_digits) {
		return false;
	}

	/* check if it's a percentage or a regular number */
	/* Don't count [+|-] in the final number */
	if (is_percentage) {
		*percent = strtof(str + has_sign, NULL);
		return true;
	} else {
		*value = strtof(str + has_sign, NULL);
		return true;
	}
}

bool is_decimal_num(const char *str)
{
	if (*str == '\0') return false;

	while (*str) {
		if (!isdigit(*str)) {
			return false;
		}
		str++;
	}

	return true;
}

/* This is really terrible */
cmdsubopts getcmdsubopt(const char *str)
{
	cmdsubopts ret = 0;

	size_t size = strlen(str);
	if (size != 3 && size != 6) {
		verbose(V_DEBUG | V_DBGMSG, "\"%s\" is not a SUBOPT\n", str);
		return ret;
	}

	if (size >= 3) {
		if (strncmp(str, "get", 3) == 0)
			ret |= SUBOPT_GET;
		if (strncmp(str, "set", 3) == 0)
			ret |= SUBOPT_SET;
	}
	if (size == 6) {
		if (strncmp(str + 3, "min", 3) == 0)
			ret |= SUBOPT_MIN;
		if (strncmp(str + 3, "max", 3) == 0)
			ret |= SUBOPT_MAX;
	}

	verbose(V_DEBUG, "getcmdsubopt(%s): %s | %s\n", str, (ret & SUBOPT_GET) ? "SUBOPT_GET" : ((ret & SUBOPT_SET) ? "SUBOPT_SET" : "0"), (ret & SUBOPT_MIN) ? "SUBOPT_MIN" : ((ret & SUBOPT_MAX) ? "SUBOPT_MAX" : "0"));

	return ret;
}

int verbose(int level, const char *fmt, ...)
{
	int ret;
	FILE *f = stdout;

	if ((verbosity & 0xF) < (level & 0xF))
		return 0;

	if (level & V_STDERR) {
		f = stderr;
		ret = fprintf(f, "%s: ", argv0);
	}
	if (level & V_WARN) {
		f = stderr;
		ret = fprintf(f, "WARNING: ");
	}
	if (level & V_DBGMSG) {
		ret = fprintf(f, "DEBUG: ");
	}

	va_list args;
	va_start(args, fmt);
	ret = vfprintf(f, fmt, args);
	va_end(args);
	return ret;
}

void print_help(cmdopts opt)
{
	if (opt == -1) {
		if (verbosity == 1) {
			/* General help */
		 	verbose(V_LOG, "Brightness Utility Tool\n");
			verbose(V_LOG, "Utility to manage device backlights and flashlights\n");
			verbose(V_LOG, "Most commands require an administrator or root user\n");
			verbose(V_LOG, "\n");
			verbose(V_LOG, "Usage:  %s [quiet] <verb> <options>, where <verb> is as follows:\n", argv0);
			verbose(V_LOG, "\n");
			verbose(V_LOG, "     backlight    (Display/Screen backlight)\n");
			verbose(V_LOG, "     keyboard     (Keyboard backlight)\n");
			verbose(V_LOG, "     flashlight   (LED Flash)\n");
			verbose(V_LOG, "\n");
			verbose(V_LOG, "%s <verb> with no options will provide light percentage on that verb\n", argv0);
		} else if (verbosity < 0 || verbosity > 3) {
			verbose(V_QUIET | V_WARN, "No help for verbosity '%d'.\n", verbosity);
		} else {
			/* 'quiet', 'verbose', 'debug' help */
			verbose(V_QUIET, "Usage:   %s %s <verb> <options>\n", argv0, (verbosity == 0) ? "quiet" : ((verbosity > 2) ? "debug" : "verbose"));
			verbose(V_QUIET, "Get/Set <verb> with <options>, %s.\n", (verbosity == 0) ? "doing all things quietly" : ((verbosity > 2) ? "logs as much as possible" : "prints more details"));
		}
	} else if (opt == CMD_BACKLIGHT) {
		verbose(V_LOG, "Usage:   %s backlight [get[max|min]|set] [percent|nits|millinits] [value] [displayID]\n", argv0);
		verbose(V_LOG, "Get/Set backlight value, getting main display brightness percent if not specified get/set and later options. While setting values, it can be directly specified as raw values which in nits, or appending a % to set as percent while no [percent|nits|millinits] specified before [value]. While getting values, [value] is not considered as an option.\n");
	} else if (opt == CMD_KBDLIGHT) {
		verbose(V_LOG, "Usage:   %s keyboard [get[max|min]|set] [value] [keyboardID]\n", argv0);
		verbose(V_LOG, "Get/Set keyboard backlight value, getting builtin keyboard brightness percent if not specified get/set and later options.\n");
	} else if (opt == CMD_FLASHLIGHT) {
		verbose(V_LOG, "Usage:   %s flashlight [get[max|min]|set] [percent|raw] [value] [LEDID]\n", argv0);
		verbose(V_LOG, "Get/Set LED level, getting main LED level percent if if not specified get/set and later options. While setting values, it can be directly specified as raw values which in levels, or appending a % to set as percent while no [percent|raw] specified before [value]. [LEDID] can be 0 to 4.\n");
	}
}

/* TODO: Proper CFRelease */
int main(int argc, char *argv[])
{
	int ret = 1;
	cmdsubopts subopt;
	float val, percent;
	unsigned long raw, rawmax, cur;
	char *display = NULL;
	char leading_sign = 0;
	bool backlight_avail, dcp;
	bool out_nits, out_millinits, out_percent, out_raw;
	out_nits = out_millinits = out_percent = out_raw = false;

	cmdopts opt = -1;
	verbosity = 1;

#ifdef DEBUG
	if (getenv("VERBOSITY")) {
		verbosity = (int)strtol(getenv("VERBOSITY"), (char **)NULL, 10);
		/* Make sure 0 < verbosity < 0x10 */
		verbosity = (verbosity >= 0xF) ? 0xF : ((verbosity <= 0) ? 0 : verbosity);
		verbose(verbosity, "Env Verbosity: %d\n", verbosity);
	}
#endif
	verbose(V_DEBUG | V_DBGMSG, "argc: %d\n", argc);

	if (argv[0])
		argv0 = argv[0];
	else
		argv0 = __progname;

	/* While `brightutil` only, defaulting to `brightutil backlight get nits primary` */
	if (argc < 2) {
default_out:
                verbose(4, "No further args specified, goto default output.\n");
		out_percent = true;
		subopt = SUBOPT_GET | SUBOPT_MAX;
		backlight_avail = backlight_ctrl(subopt, &rawmax, display);
		if (backlight_avail) {
			verbose(V_DEBUG | V_DBGMSG, "Got max brightness (%lu) of display \"%s\".\n", rawmax, (display) ? display : "0");
		} else {
			verbose(V_DEBUG | V_DBGMSG, "Cannot get max brightness of display \"%s\".\n", (display) ? display : "0");
			exit(1);
		}

		subopt = SUBOPT_GET;
		backlight_avail = backlight_ctrl(subopt, &raw, display);
		goto output_now;
	}

	if (argc >= 2) {
		int i = 1;
		/* Step 1: Check if argv[1] is verbosity control */
		/* `brightutil [verbosity] <verb>` or `brightutil help` */
		switchs(argv[i]) {
			cases("q")
			cases("quiet")
				verbosity = 0;
				i++;
				break;
			cases("v")
			cases("verbose")
				verbosity = 2;
				i++;
				break;
			cases("debug")
				verbosity = 3;
				verbose(V_DEBUG, "argv[%d]: %s\n", i, argv[i]);
				i++;
				break;
			cases("h")
			cases("help")
			cases("-h")
			cases("-help")
			cases("--h")
			cases("--help")
				print_help(-1);
				exit(1);
			/* Custom verbosity */
			defaults
				if (is_decimal_num(argv[i])) {
					verbosity = (int)strtol(argv[i], (char **)NULL, 10);
					/* Make sure 0 < verbosity < 0x10 */
					verbosity = (verbosity >= 0xF) ? 0xF : ((verbosity <= 0) ? 0 : verbosity);
					verbose(verbosity, "Verbosity: %d\n", verbosity);
					i++;
				}
				break;
		} switchs_end;

		/* Step 2: Check if argv[i] is control verbs, i=3 while verbosity specified at argv[2] */
		if (!argv[i] || i >= argc) {
			/* `brightutil [verbosity]` only, equals `brightutil [verbosity] backlight get nits primary` */
			goto default_out;
		} else {
			/* `brightutil [verbosity] [b|k|f] ...` or `brightutil [verbosity] help`. Later shows help info for [verbosity] */
			switchs(argv[i]) {
				cases("b")
				cases("bl")
				cases("backlight")
					opt = CMD_BACKLIGHT;
					break;
				cases("k")
				cases("kbd")
				cases("keyboard")
					opt = CMD_KBDLIGHT;
					break;
				cases("f")
				cases("led")
				cases("flash")
				cases("flashlight")
					opt = CMD_FLASHLIGHT;
					break;
				cases("h")
				cases("help")
				cases("-h")
				cases("-help")
				cases("--h")
				cases("--help")
					print_help(-1);
					exit(1);
				defaults
					verbose(V_LOG | V_STDERR, "did not recognize verb \"%s\"; type \"%s help\" for a list\n", argv[i], argv0);
					exit(1);
			} switchs_end;
			i++;
		}

		/* Step 3.1: Handle `brightutil [verbosity] <verb> help` */
		if (argv[i] && i < argc) {
			switchs(argv[i]) {
				cases("h")
				cases("help")
				cases("-h")
				cases("-help")
				cases("--h")
				cases("--help")
					print_help(opt);
					exit(1);
				defaults
			} switchs_end;
		}

		/* Step 3.2: Parse <verb> specific options */
		switch (opt) {
			/* backlight_ctrl(int cmdsubopt, float inputval, char __nullable *display) */
			case CMD_BACKLIGHT:
				/* `brightutil [verbosity] backlight` */
				if (!argv[i]) {
					goto default_out;
				}
				/* if next option is not [get|set][min|max], defaulting to 'get' mode */
				subopt = getcmdsubopt(argv[i]);
				if (subopt == 0) {
					subopt = SUBOPT_GET;
				} else
				/* `brightutil [verbosity] backlight [get|set][min|max]` */
				{
					i++;

					if ((subopt & SUBOPT_SET) && (!argv[i] || i >= argc)) {
						verbose(V_LOG | V_STDERR, "no value specified for option \"%s\"; type \"%s backlight help\" for usage\n", argv[i - 1], argv0);
						exit(1);
					}
				}

				/* `brightutil [verbosity] backlight [get|set][min|max] [percent|nits|millinits]` */
				/*
				 * Raw: IOMFBBrightnessLevel
				 * Nits: Raw * (2 ^ -16)
				 * Millinits: Nits * 1000.0
				 */
				if (!argv[i] || i >= argc) {
					/* `brightutil [verbosity] backlight [get|set][min|max]` */
				} else {
					out_percent = (strcmp(argv[i], "percent") == 0 || strcmp(argv[i], "%") == 0);
					out_nits = (strcmp(argv[i], "nits") == 0 || strcmp(argv[i], "nt") == 0) ;
					out_millinits = (strcmp(argv[i], "millinits") == 0 || strcmp(argv[i], "mnt") == 0);
					out_raw = (strcmp(argv[i], "raw") == 0 || strcmp(argv[i], "value") == 0);
				}
				dcp = backlight_dcp();

				/* TODO: DFR returns Nits instead of raw value, but I don't have device to test it */
				if (!dcp && out_raw) {
					verbose(V_LOG | V_STDERR, "DFR Brightness does not support setting \"%s\", use \"percent\" instead.\n", argv[i]);
					exit(1);
				}
				if (!(out_percent || out_nits || out_millinits || out_raw)) {
					/* `brightutil [verbosity] backlight [get|set][min|max] ... */
					verbose(V_DEBUG | V_DBGMSG, "No %sput format option specified, defaulting to percentage.\n", (subopt & SUBOPT_SET) ? "in" : "out");
					out_percent = true;
				} else {
					/* `brightutil [verbosity] backlight [set][min|max] [percent|nits|millinits] */
					verbose(V_DEBUG | V_DBGMSG, "Specified %sput value format \"%s\".\n", (subopt & SUBOPT_SET) ? "in" : "out", argv[i]);
					i++;
				}
				/* `brightutil [verbosity] backlight [set][min|max] [percent|nits|millinits] [value] [displayID]` */
				if ((subopt & SUBOPT_SET) && is_valid_value(argv[i], &percent, &val)) {
					/* we can't check how exact input type by just is_valid_value() */
					if (strchr(argv[i], '%')) {
						/* malformed input like `brightutil backlight set nits 100%` */
						if (out_nits || out_millinits || out_raw) {
							verbose(V_LOG | V_STDERR, "Input value \"%s\" conflicted with specified type \"%s\"", argv[i], argv[i - 1]);
							exit(1);
						}
						/* But this allows `brightutil backlight set percent 100%` */
						out_percent = true;
					}
					/* Check if input has no '%' but specified type 'percent' */
					else if (out_percent) {
						/* is_valid_value() outputs to percent only when input has '%' sign, so we manually set percent */
						percent = val;
						/* Then clear val */
						val = 0;
					}
					/* Check if has [+|-] */
					if (*argv[i] == '+' || *argv[i] == '-') {
						verbose(V_VERBOSE, "Specified %sing on current brightness value.\n", (*argv[i] == '+') ? "add" : "substract");
						leading_sign = *argv[i];
					}
					/* Check if displayID specified */
					if (argv[i + 1]) {
						verbose(V_VERBOSE, "Specified displayID \"%s\".\n", argv[i + 1]);
						display = argv[i + 1];
					}
					/* While specified %, we have to know how much is 100% */
					if (out_percent) {
						backlight_avail = backlight_ctrl(SUBOPT_GET | SUBOPT_MAX, &rawmax, display);
						if (backlight_avail) {
							verbose(V_DEBUG | V_DBGMSG, "Got max brightness (%lu) of display \"%s\".\n", rawmax, (display) ? display : "0");
						} else {
							verbose(V_DEBUG | V_DBGMSG, "Cannot get max brightness of display \"%s\".\n", (display) ? display : "0");
							exit(1);
						}
					}
					/* While specified [+|-], we have to know current value */
					if (leading_sign != 0) {
						backlight_avail = backlight_ctrl(SUBOPT_GET, &cur, display);
					}
					/* Process input value */
					if (backlight_avail || leading_sign == 0) {
						/* Convert Nits/MilliNits to raw value */
						if (out_nits) val = val / pow(2, -16);
						if (out_millinits) val = val / pow(2, -16) / 1000.0;

						if (leading_sign == '+')
							raw = cur + lrint(out_percent ? (rawmax * (percent / 100.0)) : val);
						else if (leading_sign == '-')
							raw = cur - lrint(out_percent ? (rawmax * (percent / 100.0)) : val);
						else
							raw = lrint(out_percent ? (rawmax * (percent / 100.0)) : val);
					}
				}
				/* `brightutil [verbosity] backlight [get][min|max] [percent|nits|millinits] [displayID]` */
				else if (subopt & SUBOPT_GET) {
					/* TODO: reduce repeating codes */
					/* Check if displayID specified */
					if (i < argc) {
						verbose(V_VERBOSE, "Specified displayID \"%s\".\n", argv[i]);
						display = argv[i];
					}
					/* While specified percentage, we have to know how much is 100% */
					if (out_percent) {
						backlight_avail = backlight_ctrl(SUBOPT_GET | SUBOPT_MAX, &rawmax, display);
						if (backlight_avail) {
							verbose(V_DEBUG | V_DBGMSG, "Got max brightness (%lu) of display \"%s\".\n", rawmax, (display) ? display : "0");
						} else {
							verbose(V_DEBUG | V_DBGMSG, "Cannot get max brightness of display \"%s\".\n", (display) ? display : "0");
							exit(1);
						}
					}
				}
				/* final part, call backlight_ctrl with specified options */
				backlight_avail = backlight_ctrl(subopt, &raw, display);
output_now:
				if (backlight_avail) {
					if (subopt & SUBOPT_GET) {
						if (out_nits) verbose(V_LOG, "%.2f\n", (double)(int)raw * pow(2, -16));
						/* MilliNits is stored as integer inside AppleARMBacklight */
						if (out_millinits) verbose(V_LOG, "%ld\n", lrint(raw * pow(2, -16) * 1000.0));
						if (out_percent) verbose(V_LOG, "%.2f\n", (float)raw / rawmax * 100.0);
						if (out_raw) verbose(V_LOG, "%ld\n", raw);
					}
					if (subopt & SUBOPT_SET) {
						if (out_nits || out_percent) verbose(V_VERBOSE, "Set: %.2f%s\n", out_nits ? ((double)(int)raw * pow(2, -16)) : ((float)raw / rawmax * 100.0), out_nits ? " nits" : "%");
						if (out_millinits || out_raw) verbose(V_VERBOSE, "Set: %lu%s\n", out_millinits ? lrint(raw * pow(2, -16) * 1000.0) : raw, out_millinits ? " millinits" : "");
					}
				}

				if (!backlight_avail) {
					verbose(V_LOG | V_STDERR, "Display \"%s\" is not available.\n", (display) ? display : "0");
					exit(1);
				}

				return 0;
			case CMD_KBDLIGHT:
				verbose(V_LOG | V_STDERR, "Not yet implemented.\n");
				break;
			case CMD_FLASHLIGHT:
				verbose(V_LOG | V_STDERR, "Not yet implemented.\n");
				break;
			default:
				verbose(V_LOG | V_STDERR, "How you get there?\n");
		}
	}

	return ret;
}
