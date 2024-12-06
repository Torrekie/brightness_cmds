#include <stdio.h>
#include <stdlib.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

// Recursive function to print CFDictionary properties
void printCFDictionary(CFDictionaryRef dict, int indentLevel) {
    CFIndex count = CFDictionaryGetCount(dict);
    CFTypeRef *keys = malloc(sizeof(CFTypeRef) * count);
    CFTypeRef *values = malloc(sizeof(CFTypeRef) * count);

    CFDictionaryGetKeysAndValues(dict, keys, values);

    for (CFIndex i = 0; i < count; i++) {
        for (int j = 0; j < indentLevel; j++) {
            printf("  "); // Indentation for nested dictionaries
        }

        if (CFGetTypeID(keys[i]) == CFStringGetTypeID()) {
            char keyStr[256];
            if (CFStringGetCString((CFStringRef)keys[i], keyStr, sizeof(keyStr), kCFStringEncodingUTF8)) {
                printf("%s: ", keyStr);

                CFTypeRef value = values[i];
                CFTypeID valueType = CFGetTypeID(value);

                if (valueType == CFStringGetTypeID()) {
                    char valueStr[256];
                    if (CFStringGetCString((CFStringRef)value, valueStr, sizeof(valueStr), kCFStringEncodingUTF8)) {
                        printf("%s\n", valueStr);
                    } else {
                        printf("Failed to convert CFStringRef to C string\n");
                    }
                } else if (valueType == CFNumberGetTypeID()) {
                    int num;
                    if (CFNumberGetValue((CFNumberRef)value, kCFNumberIntType, &num)) {
                        printf("%d\n", num);
                    } else {
                        printf("Failed to get number value\n");
                    }
                } else if (valueType == CFBooleanGetTypeID()) {
                    printf(CFBooleanGetValue((CFBooleanRef)value) ? "true\n" : "false\n");
                } else if (valueType == CFArrayGetTypeID()) {
                    CFArrayRef array = (CFArrayRef)value;
                    printf("[");
                    for (CFIndex j = 0; j < CFArrayGetCount(array); j++) {
                        if (j > 0) printf(", ");
                        CFTypeRef element = CFArrayGetValueAtIndex(array, j);
                        if (CFGetTypeID(element) == CFStringGetTypeID()) {
                            char elementStr[256];
                            if (CFStringGetCString((CFStringRef)element, elementStr, sizeof(elementStr), kCFStringEncodingUTF8)) {
                                printf("%s", elementStr);
                            } else {
                                printf("?");
                            }
                        }
                    }
                    printf("]\n");
                } else if (valueType == CFDictionaryGetTypeID()) {
                    printf("\n");
                    printCFDictionary((CFDictionaryRef)value, indentLevel + 1); // Recursive call
                } else if (valueType == CFDataGetTypeID()) {
                    CFDataRef data = (CFDataRef)value;
                    const UInt8 *bytes = CFDataGetBytePtr(data);
                    CFIndex length = CFDataGetLength(data);

                    printf("CFData (length = %ld): ", length);
                    for (CFIndex j = 0; j < length; j++) {
                        printf("%02X ", bytes[j]); // Print each byte as hexadecimal
                    }
                    printf("\n");
                } else {
                    // For unknown types, print the description
                    CFStringRef description = CFCopyDescription(value);
                    if (description) {
                        char descriptionStr[256];
                        if (CFStringGetCString(description, descriptionStr, sizeof(descriptionStr), kCFStringEncodingUTF8)) {
                            printf("%s\n", descriptionStr);
                        } else {
                            printf("Unknown type (failed to convert description)\n");
                        }
                        CFRelease(description);
                    } else {
                        printf("Unknown type (no description available)\n");
                    }
                }
            }
        }
    }

    free(keys);
    free(values);
}
typedef struct __IOMobileFrameBuffer *IOMobileFrameBuffer;

extern IOReturn IOMobileFramebufferGetMainDisplay(IOMobileFrameBuffer *);
extern io_registry_entry_t IOMobileFramebufferGetServiceObject(IOMobileFrameBuffer);
extern IOReturn IOMobileFramebufferOpenByName(CFStringRef, IOMobileFrameBuffer *);

int main(int argc, char *argv[]) {

    IOMobileFrameBuffer fb;
    CFStringRef display_name;
    io_service_t service;
    int ret;
    if (!argv[1]) {
        ret = IOMobileFramebufferGetMainDisplay(&fb);
        printf("IOMobileFramebufferGetMainDisplay: %d\n", ret);
    } else {
        display_name = CFStringCreateWithCString(kCFAllocatorDefault, argv[1], kCFStringEncodingUTF8);
        ret = IOMobileFramebufferOpenByName(display_name, &fb);
        printf("IOMobileFramebufferOpenByName(%s): %d\n", argv[1], ret);
    }

    if (fb) {
        service = IOMobileFramebufferGetServiceObject(fb);
        if (!service) {
            printf("IOMobileFramebufferGetServiceObject: %d\n", service);
        }
    }

    CFTypeRef properties = NULL;
    ret = IORegistryEntryCreateCFProperties(service, (CFMutableDictionaryRef *)&properties, kCFAllocatorDefault, 0);
    if (properties) {
        if (CFGetTypeID(properties) == CFDictionaryGetTypeID()) {
            printCFDictionary((CFDictionaryRef)properties, 1);
        }
    } else printf("IORegistryEntryCreateCFProperties: %d\n", ret);

    return EXIT_SUCCESS;
}
