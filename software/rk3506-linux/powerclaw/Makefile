CC ?= gcc
ARM_CC ?= arm-linux-gnueabihf-gcc
CFLAGS ?= -O2
WARNINGS := -Wall -Wextra -Werror -Wformat=2 -Wshadow -Wconversion -Wstrict-prototypes
SOURCES := src/powerclaw_device_mcp.c

.PHONY: all test arm fetch package clean

all: build/powerclaw-device-mcp

build/powerclaw-device-mcp: $(SOURCES)
	mkdir -p build
	$(CC) $(CFLAGS) $(WARNINGS) -std=c11 -o $@ $(SOURCES)

test: build/powerclaw-device-mcp
	python3 tests/test_mcp_contract.py ./build/powerclaw-device-mcp
	python3 tests/test_distribution.py .

arm: $(SOURCES)
	mkdir -p out
	$(ARM_CC) -Os $(WARNINGS) -std=c11 -static -o out/powerclaw-device-mcp.arm $(SOURCES)

fetch:
	sh scripts/fetch-zeroclaw.sh

package: test arm fetch
	sh scripts/package-release.sh

clean:
	rm -rf build out vendor
