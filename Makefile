# OmniPad PS5
# Universal Controller Engine & Web Control Center for PS5 (FW 7.00 - 13.60)
# Combines Bluetooth (AnyPad-PS5) + USB Hotplug (Ghostcontrol / PoorDS4) + ShellUI Fixes (YetAnotherControllerEnabler)

PS5_HOST ?=
PS5_PORT ?= 9021

BUILD := build

.PHONY: all pdp ps5 send clean test host-test

all: pdp

pdp:
ifndef PS5_PAYLOAD_SDK
	$(error PS5_PAYLOAD_SDK is undefined. Please export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk)
endif
	$(MAKE) -f pdp.mk

ps5:
ifndef PS5_PAYLOAD_SDK
	$(error PS5_PAYLOAD_SDK is undefined. Please export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk)
endif
	$(MAKE) -f ps5.mk

send: pdp
ifeq ($(strip $(PS5_HOST)),)
	$(error Set PS5_HOST to your console's address)
endif
	@echo "[*] Sending payload to PS5 $(PS5_HOST):$(PS5_PORT)..."
	python3 -c "import socket; f=open('dist/PDP-Faceoff-USB.elf','rb').read(); s=socket.create_connection(('$(PS5_HOST)', $(PS5_PORT)), timeout=5); s.sendall(f); s.close(); print('Payload sent')"

test: host-test

host-test:
	@mkdir -p $(BUILD)
	clang -O2 -Isrc src/usb_controllers.c src/profiles.c src/util.c src/log.c tests/test_parsers.c -o $(BUILD)/test_parsers
	./$(BUILD)/test_parsers
	clang -O2 -Isrc tests/test_vpad_abi.c -o $(BUILD)/test_vpad_abi
	./$(BUILD)/test_vpad_abi
	clang -O2 -Isrc src/usb_controllers.c src/profiles.c src/util.c src/log.c tests/test_flow_sim.c -o $(BUILD)/test_flow_sim
	./$(BUILD)/test_flow_sim
	clang -O2 -Isrc src/usb_controllers.c src/profiles.c src/util.c src/log.c tests/test_normalization.c -o $(BUILD)/test_normalization
	./$(BUILD)/test_normalization
	clang -O2 -Isrc src/usb_controllers.c src/profiles.c src/util.c src/log.c tests/test_lifecycle_logout.c -o $(BUILD)/test_lifecycle_logout
	./$(BUILD)/test_lifecycle_logout
	clang -O2 -Isrc src/usb_controllers.c src/profiles.c src/util.c src/log.c tests/test_fuzz_usb.c -o $(BUILD)/test_fuzz_usb
	./$(BUILD)/test_fuzz_usb
	clang -O2 -Isrc -pthread src/util.c src/log.c tests/test_hotplug_thrashing.c -o $(BUILD)/test_hotplug_thrashing
	./$(BUILD)/test_hotplug_thrashing
	clang -O2 -Isrc src/util.c src/log.c tests/test_web_concurrency.c -o $(BUILD)/test_web_concurrency
	./$(BUILD)/test_web_concurrency
	clang -O2 -Isrc src/usb_controllers.c src/profiles.c src/util.c src/log.c tests/test_dongle_cable_switch.c -o $(BUILD)/test_dongle_cable_switch
	./$(BUILD)/test_dongle_cable_switch

clean:
	rm -rf $(BUILD)
