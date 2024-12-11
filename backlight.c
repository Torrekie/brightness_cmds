#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <mach/mach.h>

#include <IOKit/IOKitLib.h>
//#include <Foundation/Foundation.h>
#include <CoreFoundation/CoreFoundation.h>

#include <arm_neon.h>

#include "common.h"

/* IOMobileFramebuffer.framework */
typedef struct __IOMobileFramebuffer *IOMobileFramebuffer;

static CFArrayRef (*IOMobileFramebufferCreateDisplayList)(CFAllocatorRef);
static IOReturn (*IOMobileFramebufferGetMainDisplay)(IOMobileFramebuffer *fb);
static IOReturn (*IOMobileFramebufferOpenByName)(CFStringRef name, IOMobileFramebuffer *fb);
static io_service_t (*IOMobileFramebufferGetServiceObject)(IOMobileFramebuffer fb);

/* backlight.m */
static io_service_t initIOMFB(const char *__nullable display);
static IOReturn set_value(io_service_t service, void *keys, void *values);
bool backlight_dcp(void);
bool backlight_ctrl(cmdsubopts opt, unsigned long *raw, const char *__nullable display);

static int is_dcp = -1;

static IOReturn set_value(io_service_t service, void *keys, void *values)
{
	IOReturn ret = kIOReturnError;
	kern_return_t kr;

	CFDictionaryRef dict = CFDictionaryCreate(kCFAllocatorDefault,
						  (const void **)&keys,
						  (const void **)&values,
						  1,
						  &kCFTypeDictionaryKeyCallBacks,
						  &kCFTypeDictionaryValueCallBacks);
	if (!dict) return kIOReturnNoMemory;

	kr = IORegistryEntrySetCFProperties(service, dict);
	if (kr != KERN_SUCCESS)
		verbose(V_LOG | V_STDERR, "IORegistryEntrySetCFProperties(0x%x)\n", kr);
	else
		ret = kIOReturnSuccess;

	CFRelease(dict);
	return ret;
}

static io_service_t initIOMFB(const char *__nullable display)
{
	static void *handle;
	static io_service_t service = 0;
	static IOMobileFramebuffer fb;
	static CFArrayRef array;
	static CFIndex count;
	kern_return_t ret;

	/* Avoid doing twice */
	if (service) return service;

	if (!handle) handle = dlopen("/System/Library/PrivateFrameworks/IOMobileFramebuffer.framework/IOMobileFramebuffer", RTLD_LAZY);
	if (!handle) return service;

	if (!IOMobileFramebufferCreateDisplayList) IOMobileFramebufferCreateDisplayList = dlsym(handle, "IOMobileFramebufferCreateDisplayList");
	if (!IOMobileFramebufferGetMainDisplay) IOMobileFramebufferGetMainDisplay = dlsym(handle, "IOMobileFramebufferGetMainDisplay");
	if (!IOMobileFramebufferGetServiceObject) IOMobileFramebufferGetServiceObject = dlsym(handle, "IOMobileFramebufferGetServiceObject");
	if (!IOMobileFramebufferOpenByName) IOMobileFramebufferOpenByName = dlsym(handle, "IOMobileFramebufferOpenByName");

	if (!(IOMobileFramebufferCreateDisplayList && IOMobileFramebufferGetMainDisplay && IOMobileFramebufferGetServiceObject && IOMobileFramebufferOpenByName)) {
		verbose(V_LOG | V_STDERR, "Failed to get required IOMFB functions (%s).\n", dlerror());
		return service;
	} else {
		/* TODO: Consider reject r/w if screen is disabled */
		/* Normal Mode Keys: NormalModeEnable, NormalModeActive */
		/* All prepared, then get service port */
		if (display) {
			CFStringRef cfstr;
			verbose(V_VERBOSE, "Specified display: %s\n", display);
			if (is_decimal_num(display)) {
				/* display ID */
				array = IOMobileFramebufferCreateDisplayList(kCFAllocatorDefault);
				count = CFArrayGetCount(array);
				int index = atoi(display);
				verbose(V_DEBUG | V_DBGMSG, "Display count: %ld\n", count);

				if (index >= count || index < 0) {
					verbose(V_LOG | V_STDERR, "Invalid displayID; Should between (0 ~ %ld), but got %d\n", count, index);
					exit(1);
				}

				const CFStringRef *ptr = CFArrayGetValueAtIndex(array, atoi(display));
				cfstr = *ptr;
				if (verbosity >= V_VERBOSE) {
					char *buf = calloc(1, 255);
					CFStringGetCString(cfstr, buf, 256, kCFStringEncodingUTF8);
					verbose(V_VERBOSE, "display[%d]: %s\n", index, buf);
					free(buf);
				}
			} else {
				/* display name */
				cfstr = CFStringCreateWithCString(kCFAllocatorDefault, display, kCFStringEncodingUTF8);
			}

			ret = IOMobileFramebufferOpenByName(cfstr, &fb);
			if (ret != KERN_SUCCESS) verbose(V_LOG | V_STDERR, "Failed to open display \"%s\" (%s).\n", display, mach_error_string(ret));
		} else {
			verbose(V_DEBUG | V_DBGMSG, "displayID unspecified, getting main display\n");
			ret = IOMobileFramebufferGetMainDisplay(&fb);
			if (ret != KERN_SUCCESS) verbose(V_LOG | V_STDERR, "Failed to open main display (%s).\n", mach_error_string(ret));
		}

		if (fb) {
			verbose(V_DEBUG | V_DBGMSG, "Got IOMobileFramebuffer (%p).\n", fb);
			service = IOMobileFramebufferGetServiceObject(fb);
			if (service) verbose(V_DEBUG | V_DBGMSG, "Got IOMFB Service: %ld\n", service);
		}
	}

	return service;
}

bool backlight_dcp(void)
{
	io_service_t service;
	if (is_dcp == -1) {
		/* IOService:/AppleARMPE/arm-io@XXXXXXX/AppleXXXX/dcp@XXXXXX */
		service = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceNameMatching("dcp"));
		if (service) {
			/* Only call IORegistryEntryGetPath while verbosity >= V_DEBUG */
			if (verbosity >= V_DEBUG) {
				char path[PATH_MAX];
				kern_return_t result = IORegistryEntryGetPath(service, kIOServicePlane, path);
				if (result != KERN_SUCCESS) verbose(V_DEBUG | V_DBGMSG, "Failed to get DCP path (%s)\n", mach_error_string(result));
				verbose(V_DEBUG | V_DBGMSG, "Found DCP: %s\n", path);
			}
			is_dcp = 1;
			IOObjectRelease(service);
			return true;
		}
	}

	return (is_dcp == 1);
}

bool backlight_ctrl(cmdsubopts opt, unsigned long *raw, const char *__nullable display)
{
	static long min = -1, max = -1, cur = -1;

	CFNumberRef number;
	bool ret = false;
	kern_return_t kr;

	if (is_dcp == -1)
		backlight_dcp();

	/* DCP Brightness */
	/* Requires com.apple.security.iokit-user-client-class IOMobileFramebufferUserClient */
	if (is_dcp == 1) {
		io_service_t service = initIOMFB(display);
		static io_service_t backlight;
		backlight = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceNameMatching("AppleARMBacklight"));
		if (!service) exit(1);

		if (opt & SUBOPT_GET) {
			if ((opt & SUBOPT_MIN) || (opt & SUBOPT_MAX)) {
				if (backlight) {
					/* Try get user min brightness first */
					CFDictionaryRef IODisplayParameters = NULL;
					IODisplayParameters = IORegistryEntryCreateCFProperty(backlight,
											      CFSTR("IODisplayParameters"),
											      kCFAllocatorDefault,
											      kNilOptions);
					CFDictionaryRef BrightnessMilliNits = NULL;
					if (IODisplayParameters) {
						BrightnessMilliNits = (CFDictionaryRef)CFDictionaryGetValue(IODisplayParameters, CFSTR("BrightnessMilliNits"));
					}

					if (BrightnessMilliNits) {
						/* User min millinits (Typically the min value of CC brightness control) */
						if ((opt & SUBOPT_MIN) && (number = (CFNumberRef)CFDictionaryGetValue(BrightnessMilliNits, CFSTR("min")))) {
							CFNumberGetValue(number, kCFNumberSInt64Type, &min);
							verbose(V_DEBUG | V_DBGMSG, "Got BrightnessMilliNits min: %lu\n", min);
							/* MilliNits to raw */
							min = lrint(((double)(int)min / 1000) / pow(2, -16));
							verbose(V_DEBUG | V_DBGMSG, "rounded min: %lu\n", min);
							ret = true;
						}
						/* Hardware max millinits (Typically the max value of CC brightness control when not limited by thermalmonitord) */
						if ((opt & SUBOPT_MAX) && (number = (CFNumberRef)CFDictionaryGetValue(BrightnessMilliNits, CFSTR("max")))) {
							CFNumberGetValue(number, kCFNumberSInt64Type, &max);
							verbose(V_DEBUG | V_DBGMSG, "Got BrightnessMilliNits max: %lu\n", max);
							/* MilliNits to raw */
							max = lrint(((double)(int)max / 1000.0) / pow(2, -16));
							verbose(V_DEBUG | V_DBGMSG, "rounded max: %lu\n", max);
							ret = true;
						}
						/* Don't get 'uncalMilliNits' and 'value' before we know what they actually do */
					}
					if (IODisplayParameters) CFRelease(IODisplayParameters);
				} else {
					verbose(V_DEBUG | V_DBGMSG, "Cannot find AppleARMBacklight, fallback to IOMFB.\n");
					number = IORegistryEntryCreateCFProperty(service,
										 CFSTR("BLNitsCap"), /* User max nits raw (not hardware max) */
										 kCFAllocatorDefault,
										 kNilOptions);
					if (number) {
						CFNumberGetValue(number, kCFNumberSInt64Type, &max);
						verbose(V_DEBUG | V_DBGMSG, "Got BLNitsCap: %lu\n", max);
						ret = true;
					}
				}
			} else {
				number = IORegistryEntryCreateCFProperty(service,
									 CFSTR("IOMFBBrightnessLevel"), /* Current brightness raw */
									 kCFAllocatorDefault,
									 kNilOptions);
				if (number) {
					CFNumberGetValue(number, kCFNumberSInt64Type, &cur);
					verbose(V_DEBUG | V_DBGMSG, "Got IOMFBBrightnessLevel: %lu\n", cur);
					ret = true;
				}
			}

			if (opt & SUBOPT_MIN) {
				*raw = min;
			} else if (opt & SUBOPT_MAX) {
				*raw = max;
			} else {
				*raw = cur;
			}
		} else if (opt & SUBOPT_SET) {
			if ((opt & SUBOPT_MIN) || (opt & SUBOPT_MAX)) {
unimplemented_minmax:
				/* TODO: Try to set IODisplayParameters or IOMFB */
				verbose(V_LOG | V_STDERR, "Setting Min/Max is not implemented yet.\n");
				exit(1);
			} else {
				number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, raw);
				if (number) {
					kr = IORegistryEntrySetCFProperty(service,
									  CFSTR("IOMFBBrightnessLevel"),
									  number);
					if (kr != KERN_SUCCESS) {
						verbose(V_LOG | V_STDERR, "Failed to set IOMFBBrightnessLevel (%s).\n", mach_error_string(kr));
						exit(1);
					}
					verbose(V_VERBOSE, "Set IOMFBBrightnessLevel value: %lu\n", raw);
					ret = true;
				}
			}
		} else {
			verbose(V_STDERR, "HOW YOU GET THERE?\n");
		}
	}
	/* DFR Brightness */
	else {
		verbose(V_LOG | V_STDERR, "DFR Brightness untested yet.\n");

		CFStringRef key;
		CFDictionaryRef props, value;
		io_iterator_t iterator;
		io_service_t service;

		key = CFSTR("backlight-control");
		props = CFDictionaryCreate(kCFAllocatorDefault,
					   (void *)&key,
					   (void *)&kCFBooleanTrue,
					   1,
					   &kCFTypeDictionaryKeyCallBacks,
					   &kCFTypeDictionaryValueCallBacks);

		key = CFSTR(kIOPropertyMatchKey);
		value = CFDictionaryCreate(kCFAllocatorDefault,
					   (void *)&key,
					   (void *)&props,
					   1,
					   &kCFTypeDictionaryKeyCallBacks,
					   &kCFTypeDictionaryValueCallBacks);

		// CFRelease(props);

		int displayID = 1;
		if (display) {
			CFStringRef cfstr;
			verbose(V_VERBOSE, "Specified display: %s\n", display);
			if (is_decimal_num(display)) {
				/* display ID */
				displayID = atoi(display);
				if ((unsigned int)(displayID - 1) > 1 ) {
					verbose(V_DEBUG | V_DBGMSG, "Invalid displayID (%s); Fallback to internal display (displayID:1).\n", display);
					displayID = 1;
				}
			} else {
				/* display name */
				if (strcmp("internal", display) == 0 || strcmp("default", display) || strcmp("backlight", display) == 0 || strcmp("backlight-dfr", display) == 0)
					displayID = 1;
				else if (strcmp("DFR", display) == 0 || strcmp("dfr", display) == 0)
					displayID = 2;
				else
					verbose(V_DEBUG | V_DBGMSG, "Invalid display name (%s); Fallback to internal display (displayID:1).\n", display);
			}

		} else {
			verbose(V_DEBUG | V_DBGMSG, "displayID unspecified, getting internal display\n");
		}

		if (IOServiceGetMatchingServices(kIOMasterPortDefault, value, &iterator) == KERN_SUCCESS) {
			if (iterator) {
				UInt8 buffer[4];
				CFDataRef dfr_control_bits;
				CFDictionaryRef IODisplayParameters = NULL;
				CFDictionaryRef brightness = NULL;
				unsigned int f64, i = 0;

				while ((service = IOIteratorNext(iterator))) {
					dfr_control_bits = IORegistryEntrySearchCFProperty(service,
											   kIOServicePlane,
											   CFSTR("dfr-brightness-control"),
											   kCFAllocatorDefault,
											   kIORegistryIterateRecursively | kIORegistryIterateParents);
					if (dfr_control_bits) {
						CFDataGetBytes(dfr_control_bits, CFRangeMake(0, 4), buffer);
						CFRelease(dfr_control_bits);
					}

					if ((displayID == 2 && (UInt32 *)buffer != 0) || (displayID == 1 && (UInt32 *)buffer == 0)) {
						/* TODO: Make a function for parsing IODisplayParameters */
						IODisplayParameters = IORegistryEntryCreateCFProperty(service,
												      CFSTR("IODisplayParameters"),
												      kCFAllocatorDefault,
												      kNilOptions);
						if (!IODisplayParameters) {
							verbose(V_LOG | V_STDERR, "Cannot allocate memory.\n");
							exit(1);
						}

						if (opt & SUBOPT_MAX) {
							key = CFSTR("brightness-nits-max");
							if ((opt & SUBOPT_GET) && (number = IORegistryEntryCreateCFProperty(service, key, kCFAllocatorDefault, kNilOptions)))
								CFNumberGetValue(number, kCFNumberSInt32Type, &max);
							*raw = max;
						} else if (opt & SUBOPT_MIN) {
							/* DFR does not provide a minimum nits value */
							min = 0;
							*raw = min;
						} else {
							key = CFSTR("brightness-nits");
							if ((opt & SUBOPT_GET) && (number = IORegistryEntryCreateCFProperty(service, key, kCFAllocatorDefault, kNilOptions)))
								CFNumberGetValue(number, kCFNumberSInt32Type, &cur);
							*raw = cur;
						}
						if (opt & SUBOPT_SET) {
							if (opt & SUBOPT_MAX || opt & SUBOPT_MIN) {
								goto unimplemented_minmax;
							}

							f64 = vcvtd_n_s64_f64(rint(*raw), 10); // TODO: Replace this with portable code
							number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &f64);
							kr = set_value(service, &key, &number);
							if (kr != KERN_SUCCESS) {
								verbose(V_LOG | V_STDERR, "Unable to set value (%s).\n", mach_error_string(kr));
								exit(1);
							}
						}
						i++;
					}
				}
				if (i == 0) {
					verbose(V_LOG | V_STDERR, "No entries under display \"%s\"\n", display);
					exit(1);
				}
			} else {
				verbose(V_LOG | V_STDERR, "Cannot open display \"%s\"\n", display);
				exit(1);
			}
		} else {
			/* TODO: Get/Set by BrightnessControl.framework */
			verbose(V_LOG | V_STDERR, "Neither DCP nor DFR detected, not sure what brightness backend this device using.\n");
			exit(1);
		}
	}

	return ret;
}
