#include "ps5_vpad.h"
#include "usb_hotplug.h"
#include "log.h"
#include "util.h"
#include "web_pdp.h"

#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>

#ifdef __PROSPERO__
#include <ps5/kernel.h>
#endif

#define STATE_DIR "/data/pdp-pad"
#define STOP_FLAG STATE_DIR "/stop"

static volatile sig_atomic_t running = 1;
static atomic_int stop_requested;

void pdp_request_stop(void)
{
    atomic_store(&stop_requested, 1);
}

static void stop_signal(int sig)
{
    (void)sig;
    running = 0;
}

int main(void)
{
    signal(SIGINT, stop_signal);
    signal(SIGTERM, stop_signal);
    signal(SIGPIPE, SIG_IGN);
    mkdir(STATE_DIR, 0755);
    /* Advisory lock is released automatically on process exit. Never kill
     * a PID from a stale file, and never load two instances of this build. */
    int lock_fd = open(STATE_DIR "/instance.lock", O_RDWR | O_CREAT, 0600);
    if (lock_fd < 0 || flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
        puts("PDP payload: state directory unavailable or another instance is running");
        if (lock_fd >= 0) close(lock_fd);
        return 1;
    }
    log_init(STATE_DIR "/pdp-pad.log");
    log_line("PDP Faceoff USB payload with dashboard; no Bluetooth or autoload");

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
    if (!pdp_web_init(8096)) log_line("PDP dashboard failed to start");
    log_line("Ready. Stop by creating " STOP_FLAG " or sending SIGTERM.");
    while (running && !atomic_load(&stop_requested)) {
        long now = now_ms();
        vpad_poll(now);
        usb_hotplug_poll(now);
        if (access(STOP_FLAG, F_OK) == 0) {
            unlink(STOP_FLAG);
            running = 0;
        }
        usleep(4000);
    }
    pdp_web_cleanup();
    usb_hotplug_cleanup();
    vpad_cleanup_all();
    log_line("PDP payload stopped cleanly");
    log_close();
    close(lock_fd);
    return 0;
}
