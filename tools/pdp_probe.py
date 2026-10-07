"""Read changed input reports from the PDP 0e6f:0184 controller."""

import time
import hid


VID = 0x0E6F
PID = 0x0184


def main():
    devices = hid.enumerate(VID, PID)
    for device in devices:
        print({key: device.get(key) for key in ("path", "product_string", "usage_page", "usage", "interface_number")}, flush=True)
    if not devices:
        raise SystemExit("Controller not detected")

    device = hid.device()
    device.open_path(devices[0]["path"])
    device.set_nonblocking(True)
    print("Capturing changed reports for 45 seconds. Press buttons and move sticks now.", flush=True)
    last = None
    end = time.monotonic() + 45
    try:
        while time.monotonic() < end:
            report = bytes(device.read(64))
            if report and report != last:
                print(f"{time.monotonic():.3f} {report.hex(' ')}", flush=True)
                last = report
            time.sleep(0.005)
    finally:
        device.close()


if __name__ == "__main__":
    main()
