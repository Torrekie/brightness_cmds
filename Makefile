.PHONY: clean

PREFIX := /usr/local

TARGET := arm64-apple-ios13.0
SDKROOT := $(shell xcrun -sdk iphoneos --show-sdk-path)

CODESIGN := $(shell command -v codesign 2> /dev/null)
ENTARG := -f -s - --entitlements 

ifndef CODESIGN
	CODESIGN := ldid
	ENTARG := -S
endif

export IPHONEOS_DEPLOYMENT_TARGET = 13.0

brightutil:
	$(CC) -isysroot $(SDKROOT) -target $(TARGET) -fobjc-arc $(CFLAGS) main.c backlight.c -o brightutil $(LDFLAGS) -framework CoreFoundation -framework IOKit
	$(CODESIGN) $(ENTARG)brightness.entitlements brightutil

all: brightutil

install:
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m755 brightutil $(DESTDIR)$(PREFIX)/bin/brightutil

clean:
	rm -f *.o
	rm -f getbrt
