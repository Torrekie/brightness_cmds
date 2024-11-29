TARGET := arm64-apple-ios13.0
SDKROOT := $(shell xcrun -sdk iphoneos --show-sdk-path)

export IPHONEOS_DEPLOYMENT_TARGET = 13.0

getbrt:
	$(CC) -isysroot $(SDKROOT) -target $(TARGET) -fobjc-arc $(CFLAGS) getbrt.m -o getbrt $(LDFLAGS) -framework CoreFoundation -framework IOKit

all: getbrt
