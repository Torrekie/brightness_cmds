#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

typedef struct __IOMobileFramebuffer *IOMobileFramebuffer;
static CFArrayRef (*IOMobileFramebufferCreateDisplayList)(CFAllocatorRef);
static IOReturn (*IOMobileFramebufferGetMainDisplay)(IOMobileFramebuffer *fb);
static IOReturn (*IOMobileFramebufferOpenByName)(CFStringRef name, IOMobileFramebuffer *fb);
static io_service_t (*IOMobileFramebufferGetServiceObject)(IOMobileFramebuffer fb);

int main(int argc, char *argv[]) {
	void *handle = dlopen ("/System/Library/PrivateFrameworks/IOMobileFramebuffer.framework/IOMobileFramebuffer", RTLD_LAZY);
	if (!IOMobileFramebufferCreateDisplayList) IOMobileFramebufferCreateDisplayList = dlsym (handle, "IOMobileFramebufferCreateDisplayList");
	if (!IOMobileFramebufferGetMainDisplay) IOMobileFramebufferGetMainDisplay = dlsym (handle, "IOMobileFramebufferGetMainDisplay");
	if (!IOMobileFramebufferGetServiceObject) IOMobileFramebufferGetServiceObject = dlsym (handle, "IOMobileFramebufferGetServiceObject");
	if (!IOMobileFramebufferOpenByName) IOMobileFramebufferOpenByName = dlsym (handle, "IOMobileFramebufferOpenByName");

	CFArrayRef array;
	CFIndex count;
	io_service_t service;
	IOReturn ret;
	IOMobileFramebuffer fb;

	if (argc < 2)
		ret = IOMobileFramebufferGetMainDisplay (&fb);
	else
		ret = IOMobileFramebufferOpenByName (CFStringCreateWithCString (kCFAllocatorDefault, argv[1], kCFStringEncodingUTF8), &fb);
	if (ret != kIOReturnSuccess) return EXIT_FAILURE;

	service = IOMobileFramebufferGetServiceObject (fb);
	if (!service) return EXIT_FAILURE;

	CFBooleanRef NormalModeActive, NormalModeEnable;
	NormalModeActive = IORegistryEntryCreateCFProperty (service, CFSTR("NormalModeActive"), kCFAllocatorDefault, kNilOptions);
	NormalModeEnable = IORegistryEntryCreateCFProperty (service, CFSTR("NormalModeEnable"), kCFAllocatorDefault, kNilOptions);
	IOObjectRelease(service);

	printf("%sabled / %sactive\n", (CFBooleanGetValue(NormalModeEnable)) ? "en" : "dis", (CFBooleanGetValue(NormalModeActive)) ? "" : "in");

	return EXIT_SUCCESS;
}
