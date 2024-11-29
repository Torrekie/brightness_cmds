#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include <IOKit/IOKitLib.h>
#include <Foundation/Foundation.h>
#include <CoreFoundation/CoreFoundation.h>

typedef struct __IOMobileFramebuffer *IOMobileFramebuffer;

/* TODO: Proper CFRelease */
int main(int argc, char *argv[])
{
	int ret = 1;
	CFStringRef key;
	CFDictionaryRef props, value, param;
	CFDictionaryRef brightness;
	CFNumberRef number;
	SInt64 cur, min, max;
	io_service_t service;

	/* DCP Brightness */
	/* Requires com.apple.security.iokit-user-client-class IOMobileFramebufferUserClient */
	service = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceNameMatching("dcp"));
	if (service) {
		IOObjectRelease(service);
		void *handle = dlopen("/System/Library/PrivateFrameworks/IOMobileFramebuffer.framework/IOMobileFramebuffer", RTLD_LAZY);
		void *symbol;
		if (!handle) goto done;

		CFArrayRef (*IOMobileFramebufferCreateDisplayList)();
		IOReturn (*IOMobileFramebufferGetMainDisplay)(IOMobileFramebuffer *fb);
		IOReturn (*IOMobileFramebufferOpenByName)(CFStringRef name, IOMobileFramebuffer *fb);
		io_service_t (*IOMobileFramebufferGetServiceObject)(IOMobileFramebuffer fb);

		IOMobileFramebufferCreateDisplayList = dlsym(handle, "IOMobileFramebufferCreateDisplayList");
		IOMobileFramebufferGetMainDisplay = dlsym(handle, "IOMobileFramebufferGetMainDisplay");
		IOMobileFramebufferGetServiceObject = dlsym(handle, "IOMobileFramebufferGetServiceObject");
		IOMobileFramebufferOpenByName = dlsym(handle, "IOMobileFramebufferOpenByName");

		IOMobileFramebuffer fb;

		ret = IOMobileFramebufferGetMainDisplay(&fb);
		if (ret != KERN_SUCCESS) {
			printf("%d\n", ret);
			goto done;
		}
		printf("%p\n", fb);
		if (fb) {
			service = IOMobileFramebufferGetServiceObject(fb);
			printf("IOMobileFramebufferGetServiceObject: %d\n", service);
		}

		/* This is the most sad part, IOMFBBrightnessLevel is a NSNumber, which not compatible with C */
		number = (__bridge CFNumberRef)(__bridge NSNumber *)IORegistryEntryCreateCFProperty(service,
									       CFSTR("IOMFBBrightnessLevel"),
									       kCFAllocatorDefault,
									       kNilOptions);
		CFNumberGetValue(number, kCFNumberSInt64Type, &cur);
		/* 1 nit = 65536 = (2 ^ 16), max nits = 32768 nits = INT_MAX, so multiple with (2 ^ -16) for value in nits */
		printf("%.2f\n", (double)(int)cur * (2 ^ -16));
	}
	/* DFR Brightness */
	else {
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

		service = IOServiceGetMatchingService(kIOMasterPortDefault, value);
		if (service) {
			param = (CFDictionaryRef)IORegistryEntryCreateCFProperty(service,
										 CFSTR("IODisplayParameters"),
										 kCFAllocatorDefault,
										 kNilOptions);
			if (!param) {
				fprintf(stderr, "can't allocate memory\n");
				goto done;
			}

			brightness = CFDictionaryGetValue(param, CFSTR("brightness"));
			if (!brightness) goto done;

			if ((number = CFDictionaryGetValue(brightness, CFSTR("value"))))
				CFNumberGetValue(number, kCFNumberSInt32Type, &cur);

			if ((number = CFDictionaryGetValue(brightness, CFSTR("min"))))
				CFNumberGetValue(number, kCFNumberSInt32Type, &min);

			if ((number = CFDictionaryGetValue(brightness, CFSTR("max"))))
				CFNumberGetValue(number, kCFNumberSInt32Type, &max);

			/* DFR has max value 65536 (2 ^ 16), which should output the pct */
			printf("%.6f", (double)cur * (2 ^ -16));
			// printf("Current: %lld, Min: %lld, Max: %lld\n", cur, min, max);
		}
	}

done:
	return ret;
}
