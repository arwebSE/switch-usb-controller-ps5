#include "ps5_vpad.h"
#include "usb_hotplug.h"
#include "log.h"
#include "util.h"

#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef __PROSPERO__
#include <ps5/kernel.h>
#endif

#define STATE_DIR "/data/pdp-pad"
#define STOP_FLAG STATE_DIR "/stop"

static volatile sig_atomic_t running = 1;

static void stop_signal(int sig)
{
    (void)sig;
    running = 0;
}

int main(void)
{
    signal(SIGINT, stop_signal);
    signal(SIGTERM, stop_signal);
    mkdir(STATE_DIR, 0755);
    log_init(STATE_DIR "/pdp-pad.log");
    log_line("PDP Faceoff USB-only payload; no Bluetooth, web UI or autoload");

#ifdef __PROSPERO__
    uint32_t fw = kernel_get_fw_version();
    log_line("Firmware: 0x%08x", fw);
    if (fw < 0x07000000 || fw > 0x1360ffff) {
        log_line("Unsupported firmware; exiting without touching virtual pads");
        log_close();
        return 1;
    }
#endif

    if (!vpad_init()) {
        log_line("Virtual pad initialization failed");
        log_close();
        return 1;
    }
    usb_hotplug_init();
    log_line("Ready. Stop by creating " STOP_FLAG " or sending SIGTERM.");
    while (running) {
        long now = now_ms();
        vpad_poll(now);
        usb_hotplug_poll(now);
        if (access(STOP_FLAG, F_OK) == 0) {
            unlink(STOP_FLAG);
            running = 0;
        }
        usleep(4000);
    }
    usb_hotplug_cleanup();
    vpad_cleanup_all();
    log_line("PDP payload stopped cleanly");
    log_close();
    return 0;
}
