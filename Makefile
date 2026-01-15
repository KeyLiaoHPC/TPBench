.PHONY: all configure libtpbench libtriad tpbcli clean

BUILD_DIR ?= build
BUILD_TYPE ?= Release

all: libtpbench tpbcli

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

libtpbench: configure
	cmake --build $(BUILD_DIR) --target tpbench

libtriad: configure
	cmake --build $(BUILD_DIR) --target triad

tpbcli: configure
	cmake --build $(BUILD_DIR) --target tpbcli

clean:
	rm -rf $(BUILD_DIR)
