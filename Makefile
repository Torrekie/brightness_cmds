.PHONY: clean

TARGET := arm64-apple-ios13.0
SDKROOT := $(shell xcrun -sdk iphoneos --show-sdk-path)

CODESIGN := codesign -f -s -
ENTARG := --entitlements 

export IPHONEOS_DEPLOYMENT_TARGET = 13.0

getbrt:
	$(CC) -isysroot $(SDKROOT) -target $(TARGET) -fobjc-arc $(CFLAGS) getbrt.m -o getbrt $(LDFLAGS) -framework CoreFoundation -framework IOKit
	$(CODESIGN) $(ENTARG)brightness.entitlements getbrt

all: getbrt

clean:
	rm -f *.o
	rm -f getbrt
