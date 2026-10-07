include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk

ELF := dist/PDP-Faceoff-USB.elf
BUILD := build/pdp
CFLAGS := -std=c11 -Wall -Wextra -O2 -ffunction-sections -fdata-sections -DPDP_ONLY -Isrc
LDFLAGS += -Wl,--gc-sections
LDLIBS += -lScePad -lSceUserService -lSceSystemService -lSceAppInstUtil -ldl

SRCS := src/util.c src/log.c src/usb_controllers.c src/usb_hotplug.c \
        src/shellui_inject.c src/ps5_vpad.c src/web_pdp.c src/main_pdp.c
OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))
HEADERS := $(wildcard src/*.h)

$(ELF): $(OBJS)
	@mkdir -p dist
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	@echo "Built $@"

$(BUILD)/%.o: src/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -rf $(BUILD)
