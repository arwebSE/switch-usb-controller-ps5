# SwitchBridge PS5

A lightweight bridge for using a wired Switch controller on a jailbroken PS5, with a small web dashboard for managing input and profiles.

**Currently supports the PDP Faceoff Deluxe+ Audio Wired Controller for Nintendo Switch (`0e6f:0184`) only.** The repository and ELF retain their original names so existing download links and Payload Manager entries keep working.

[Download the ELF](https://github.com/arwebSE/ps5-pdp-pad/releases/latest/download/PDP-Faceoff-USB.elf) · [Releases](https://github.com/arwebSE/ps5-pdp-pad/releases) · [Report an issue](https://github.com/arwebSE/ps5-pdp-pad/issues) · [GPL-3.0 license](LICENSE)

## Compatibility

- **Controller:** PDP Faceoff Deluxe+ Audio Wired, USB ID `0e6f:0184`.
- **Connection:** wired USB only. Similar-looking controllers with a different USB ID are not supported by this build.
- **Verified console firmware:** PS5 **12.60**. USB discovery, sticks, buttons, D-pad, separate-profile binding, and the dashboard were tested on a real console. Successful live use was confirmed.
- **Not yet verified:** individual games and emulators, other firmware versions, multiple PDP controllers, and longer-term DualSense coexistence.

The payload requires an already-working jailbreak and an ELF loader or Payload Manager. It does not install a jailbreak or update the console's firmware. The firmware check in the source is not a compatibility guarantee for every version it accepts.

## What it includes

- PDP input mapped to a virtual PS5 controller.
- USB hotplug and a reader that preserves D-pad press/release reports.
- Home/PS and touchpad-click mappings.
- A LAN dashboard on **port 8096**: input status, profile assignment, Home, release/reconnect, logs, and Stop.
- Optional profile protection and a persistent pause state.
- A process-lifetime lock to prevent duplicate instances of this version.

There is **no Bluetooth scanning or pairing**, TCP input streaming, shell-process injection, or automatic payload loading in the dedicated build. Rumble, motion input, adaptive triggers, touchpad movement, and controller audio are not implemented. ZL/ZR act as digital triggers.

## First-time setup

**Use a separate console user for the PDP. Do not assign it to the user who owns your DualSense.** Name the second user something recognizable, such as `SwitchCtl`.

When using both controllers, start paused and protect the DualSense profile before enabling PDP input. Native controller detection can be unavailable from the payload process, so automatic detection alone is not sufficient.

1. Create the separate PDP user on the PS5. Confirm that the DualSense works on your normal user.
2. Stop AnyPad, OmniPad, and any previous PDP payload instance. Do not run controller payloads together during setup.
3. Through your PS5 FTP server, create `/data/pdp-pad/` if needed and upload an empty file named `pause` inside it. This prevents virtual-pad creation on startup.
4. Download `PDP-Faceoff-USB.elf` from [Releases](https://github.com/arwebSE/ps5-pdp-pad/releases) and load it manually. Plug in the PDP.
5. Open `http://<PS5_IP>:8096/`. With the DualSense user signed in, open `http://<PS5_IP>:8096/api/state` and find that user's name and eight-digit `id` in the `users` list.
6. Create a plain-text file named `native-user` containing only that ID, then upload it to `/data/pdp-pad/native-user`. Click **Stop payload** and reload the ELF: profile configuration is read at startup, and the pause file remains in place.
7. Click **Reconnect**, then press the PDP's **Home** button or the dashboard's **Press Home / PS** button. When the console asks for a user, choose the separate PDP user, **not** the DualSense user.
8. Confirm that the dashboard says **Assigned to** the PDP profile and that input works. Check the DualSense on its own profile too.

For a fixed automatic assignment on later loads, save the PDP profile's ID from `/api/state` in `/data/pdp-pad/pdp-user` and restart the payload once more. See [Profile configuration](#profile-configuration).

On subsequent sessions, manually load the ELF and select the PDP profile when prompted. If you previously released the virtual pad, click **Reconnect** first. Use Home/PS to request controller focus if needed; game-specific focus behavior has not been explicitly verified.

### Payload Manager

For the Payload Manager layout used during testing, store the ELF and its optional [metadata](payload/PDP-Faceoff-USB.elf.json) together:

```text
/data/pldmgr/payloads/PDP-Faceoff-USB/
├── PDP-Faceoff-USB.elf
└── PDP-Faceoff-USB.elf.json
```

Stop the running instance before loading a replacement. Updating the saved ELF does not replace the process already running in memory. Older builds do not participate in the new instance lock and must also be stopped first.

## Dashboard

Open the following address from a phone or computer on the same network:

```text
http://<PS5_IP>:8096/
```

| Control | Behavior |
| --- | --- |
| Press Home / PS | Sends a short PS-button press through our active virtual pad. |
| Assign profile | Binds our pad to a selected signed-in user. Protected profiles and profiles reporting a connected native controller are rejected. |
| Release virtual pad | Requests removal of our virtual pad and saves the pause state. The dashboard and USB reader stay running. |
| Reconnect | Clears the pause state and queues a virtual pad for the connected PDP. |
| Refresh log | Shows the last 12 KB of the payload log. |
| Stop payload | Stops the payload and cleans up its known virtual pad. The dashboard then goes offline. |

**Release is persistent:** unplugging/replugging the PDP or reloading the payload does not resume a released pad until you click Reconnect. A release during device identification may take a moment to complete.

The dashboard also shows sticks, button bits, the assigned user, and signed-in profiles. **Native connection status unavailable** means the payload could not read that status; it does **not** mean your DualSense is disconnected. The dashboard cannot repair Bluetooth pairing or restart the console's controller services.

**LAN only:** the dashboard has no authentication. Do not port-forward it or expose it through a public reverse proxy. Control requests require a custom header and cross-origin access is not enabled, but those measures are not a substitute for authentication.

## Button mapping

Face buttons follow their physical positions, not their printed letters.

| PDP control | PS5 control |
| --- | --- |
| B, bottom face button | Cross / X / confirm |
| A, right face button | Circle / back |
| Y, left face button | Square |
| X, top face button | Triangle |
| L / R | L1 / R1 |
| ZL / ZR | L2 / R2, digital |
| Minus / Plus | Create / Options |
| Left / right stick click | L3 / R3 |
| Home | PS |
| Capture | Touchpad click |
| D-pad and sticks | Corresponding directions and axes |

## Profile configuration

These optional plain-text files are read **when the payload starts**:

| File | Purpose |
| --- | --- |
| `/data/pdp-pad/native-user` | Reserves the DualSense user's ID. The payload refuses to programmatically bind its pad to this user. |
| `/data/pdp-pad/pdp-user` | Sets the preferred PDP user's ID for automatic binding. If that user is not signed in, the virtual pad waits unbound until the separate user signs in. |

Each file contains one eight-digit hexadecimal user ID, without quotes. Get the actual IDs from the dashboard's `/api/state` response while those users are signed in. **IDs are console-specific; do not copy another console's values.** Invalid IDs or identical native/PDP IDs cause startup to be refused.

The preferred PDP ID controls **automatic** assignment. The dashboard can still explicitly rebind to another eligible signed-in user. Native-user protection does not override choices made in the console's own profile picker: you must still select the correct user there.

## Troubleshooting

### Dashboard will not open

Confirm the payload is running, use the console's current LAN IP, and include `:8096` with **HTTP**, not HTTPS. Check that the computer or phone can reach the console on the same network. The dashboard closes when you stop the payload or restart the PS5.

### USB connected, but the virtual pad is inactive

Click **Reconnect** if the pad was released. If creation fails, inspect **Refresh log**. A different PDP model or USB ID is not supported. If the pad is unassigned, press Home and select the separate console user.

### Works in menus, but not in a game

Check that the pad belongs to the separate PDP user, then try Home/PS to request focus. Do not move it to the DualSense user's profile as a workaround. Game and emulator compatibility is not yet explicitly verified; include the title, firmware, and relevant log entries when reporting a failure.

### D-pad taps are skipped or repeated

Use the current release and stop any older instance. The USB-reader fix preserves both press and release reports; the corrected build was confirmed to move one menu item per tap. If it recurs, report whether sticks and face buttons also misbehave and include the log around the event.

### DualSense flashes blue and will not connect

During development, native controllers stopped connecting after a session involving AnyPad and PDP builds. Stopping our payload did not recover them; a **full console reboot** did. Native input then worked after the jailbreak was reactivated, and subsequent live PDP use with separate profiles was confirmed. The original cause remains unconfirmed.

Stop the PDP payload, unplug the PDP, fully shut down the console rather than using Rest Mode, and test the DualSense before reloading controller payloads. If pairing still fails, follow [Sony's controller-reset instructions](https://www.playstation.com/en-us/support/hardware/troubleshoot-dualsense/) and reconnect with a USB data cable. Do not update firmware merely to troubleshoot this payload if you need to preserve your jailbreak.

### Need to stop without the dashboard

Upload an empty file to `/data/pdp-pad/stop` through FTP. The payload consumes the file and exits cleanly. The `instance.lock` file is an advisory process lock; its presence alone does not mean an instance is running.

## Build from source

The Docker image includes Clang and **PS5 Payload SDK v0.43**. Run these commands from the repository root using Docker with Linux containers.

Linux:

```sh
docker build -t ps5-pdp-pad-builder .
docker run --rm -v "$PWD:/work" -w /work ps5-pdp-pad-builder
```

Windows PowerShell:

```powershell
docker build -t ps5-pdp-pad-builder .
docker run --rm -v "${PWD}:/work" -w /work ps5-pdp-pad-builder
```

Output: **`dist/PDP-Faceoff-USB.elf`**.

With the SDK already installed on Linux:

```sh
export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk
make -f pdp.mk
```

Use `pdp.mk` or the Docker image's default command for this project. Inherited `make ps5` targets and `tools/build-elf-docker.*` scripts build the upstream OmniPad variant instead. Other controller parsers and upstream documentation retained in the repository are not compatibility claims for the dedicated PDP ELF.

### Host tests

The PDP tests cover USB report parsing, dashboard HTTP behavior, virtual-device ownership, profile protection, and cleanup. Run them in a disposable Linux container: the dashboard tests create `/data/pdp-pad` **inside that container**. They do not connect to a PS5.

Start a shell in the builder image:

```sh
docker run --rm -it -v "$PWD:/work:ro" -w /work ps5-pdp-pad-builder sh
```

Then run:

```sh
clang -std=c11 -Wall -Wextra -Werror -Isrc \
  tests/test_pdp.c src/usb_controllers.c -o /tmp/pdp-parser-test
/tmp/pdp-parser-test

clang -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined \
  -g -Isrc tests/test_pdp_dashboard.c -o /tmp/pdp-dashboard-test
/tmp/pdp-dashboard-test

clang -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined \
  -g -Isrc tests/test_pdp_lifecycle.c src/util.c -ldl -o /tmp/pdp-lifecycle-test
/tmp/pdp-lifecycle-test
```

## Implementation and safety boundaries

The dedicated build uses `PDP_ONLY` and [its own entry point](src/main_pdp.c). It filters USB devices to `0e6f:0184` before claiming endpoints, identifies its virtual device through kernel-log events, and uses local virtual-pad/MBus APIs. Shell-process injection and startup sweeps of other virtual pads are disabled. If kernel-log identification is unavailable, it refuses device creation instead of falling back to a native controller handle.

The payload uses elevated process credentials and shared console controller services. These restrictions reduce its scope; they are not a guarantee against crashes or controller-service problems. Load it manually, keep a working native-controller baseline, and avoid running competing controller payloads.

Runtime files live in `/data/pdp-pad/`: the log (`pdp-pad.log`), pause/stop markers, profile configuration, and instance lock. No autoload entry is installed by this build.

## Credits and license

This is a focused derivative of [OmniPad PS5 by diegobarbosaa](https://github.com/diegobarbosaa/OmniPad-PS5). Its upstream history and source attribution are retained. The README documents the dedicated PDP build rather than reproducing OmniPad's broader feature list.

Thanks to the upstream projects and contributors:

- **sinfiltros** — [AnyPad-PS5](https://github.com/sinfiltros/AnyPad-PS5).
- **StonedModder / a-ddr** — PoorDS4 and Ghostcontrol; [PoorDS4 repository](https://github.com/a-ddr/PoorDS4).
- **MegaCadeDev** — [YetAnotherControllerEnabler](https://github.com/MegaCadeDev/YetAnotherControllerEnabler).
- The [ps5-payload-sdk contributors](https://github.com/ps5-payload-dev/sdk), and the kstuff, etaHEN, and wider PS5 homebrew communities.

Licensed under **GNU GPL v3.0**; see [LICENSE](LICENSE). This is an independent hardware-interoperability project, not affiliated with Sony, Nintendo, or PDP. Product names belong to their respective owners.
