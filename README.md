# ZBook Bring-up Firmware

Bring-up firmware for the **ZBook** board (RP2350B), built on the **Zephyr** RTOS.
It validates every hardware module on the board through an interactive shell
exposed over the UART.

This repository is the **application + west manifest**. The board itself lives in
a separate Zephyr module, [`zephyr-book/zbook`](https://github.com/zephyr-book/zbook),
pulled in by `west.yml` — so the app does not hardcode `BOARD_ROOT`.

---

## Tested modules

The shell root command is `test`. Run `test <module>`:

Available on **both revisions**:

| Shell command   | Module                                                      |
|-----------------|-------------------------------------------------------------|
| `test ldr`      | LDR light sensor via ADC                                    |
| `test io`       | GPIOs — indicator LEDs and buttons (plus the white LED on P2)|
| `test pwm`      | PWM — buzzer with frequency driven by a potentiometer       |
| `test addr_led` | WS2812 addressable LEDs via PIO (1 pixel on P1, 4 on P2)    |
| `test display`  | SH1106 OLED — 3-wire SPI on P1, i2c0 on P2                  |
| `test ir_io`    | IR emitter and receiver (manual loopback; `send`/`recv` verify two boards, see below) |
| `test sd`       | SD card over SPI (FAT32)                                    |
| `test motor`    | MOSFET PWM output — DC motor on P2, generic MOSFET on P1    |
| `test all`      | Runs every test available on the revision being built       |

**P2 only** — these subcommands do not exist in a P1 build, because the
devicetree nodes they need are not there:

| Shell command   | Module                                                      |
|-----------------|-------------------------------------------------------------|
| `test temp`     | TMP1075 temperature sensor on i2c0                          |
| `test accel`    | BMI323 6-axis IMU on spi1                                   |
| `test hall`     | TMAG5213 hall sensor (digital output)                       |
| `test encoder`  | PEC12R rotary encoder (gpio-qdec) + push-switch             |
| `test mic`      | Electret microphone via ADC (peak-to-peak per window)       |

Run `test -h` on the target to see what the running image actually has.

### Verifying IR between two boards

`test ir_io` on its own is a manual loopback (toggle the emitter with a
button, watch the receiver mirror it on an LED) — it can't tell you whether
one board's emitter can actually reach *another* board's receiver. For that,
flash the same image to two ZBook units and run one subcommand on each:

```text
Board A: test ir_io recv        # listens for 0xA5 for 15s
Board B: test ir_io send        # transmits 0xA5 repeatedly for 15s
```

Point board B's IR emitter at board A's receiver a few cm apart. Board A
prints `PASS` as soon as it decodes a matching frame, or `FAIL` after the 15s
window if nothing valid came through. Both subcommands take an optional byte
argument (`test ir_io send 0x42`, `test ir_io recv 0x42`) if you want to rule
out a value that happens to work by coincidence.

### Writing a new test

Each test lives in its own `src/<name>_test.c` and registers its own
subcommand from that file:

```c
SHELL_SUBCMD_ADD((test), mytest, NULL, "help text", cmd_test_mytest, 1, 0);
```

Wrap the file body in `#if DT_NODE_EXISTS(...)` to make it revision-specific.
Registration has to happen per-file rather than from one array in `main.c`,
because a preprocessor conditional cannot legally appear inside a macro
argument list.

---

## Prerequisites

- [West](https://docs.zephyrproject.org/latest/develop/west/install.html) installed
- The `arm-zephyr-eabi` toolchain (Zephyr SDK) configured
- The **Raspberry Pi build of OpenOCD** (it has the `rp2350` target support that
  stock OpenOCD lacks), plus a CMSIS-DAP probe (Raspberry Pi Debug Probe, or
  another Pico as an SWD probe). See [Flash via OpenOCD](#flash-via-openocd).
- Python 3.11+

---

## Workspace setup

This repo is the west manifest, so initialize the workspace from it. `west update`
then fetches Zephyr, the modules, and the `zbook` board module:

```bash
# First time — initialize the workspace from this repo
west init -m git@github.com:zephyr-book/bring-up-test-p1.git --mr main zephyr-book-workspace
cd zephyr-book-workspace
west update
```

Resulting layout (west T2 — application is the manifest repo):

```
zephyr-book-workspace/
├── bring-up-test-p1/   # this repo (app + west.yml manifest)
├── zbook/              # zephyr-book/zbook (board support + snippets, Zephyr module)
└── deps/               # zephyr + modules (hal_rpi_pico, cmsis_6, fatfs, littlefs,
                        #   oled fonts/display)
```

Run all build/flash commands from inside `bring-up-test-p1/`. After changing
`west.yml`, re-run `west update`.

---

## Build configurations

Wi-Fi, persistent credentials and the bootloader layout are all **opt-in** and
selected at build time. The base build has no networking and boots standalone.
The board (`zbook/rp2350b/m33`) is provided by the `zbook` module — pass it with
`-b`; no `BOARD_ROOT` is needed.

| Goal                                  | Command                                                                       |
|---------------------------------------|-------------------------------------------------------------------------------|
| Base (standalone, no Wi-Fi)           | `west build -p always -b zbook/rp2350b/m33`                                   |
| + Wi-Fi (ESP8266 module)              | `west build -p always -b zbook/rp2350b/m33 --shield zbook_wifi`               |
| + persistent Wi-Fi credentials (NVS)  | `west build -p always -b zbook/rp2350b/m33 --shield zbook_wifi -S wifi-credentials` |
| + persistent Wi-Fi credentials (LittleFS) | `west build -p always -b zbook/rp2350b/m33 --shield zbook_wifi -S zbook-wifi-credentials-littlefs` |
| MCUboot layout (app in slot-0)        | `west build -p always -b zbook/rp2350b/m33/mcuboot --sysbuild`                |

Notes:

- **`-p always`** forces a pristine (clean) reconfigure. It is required whenever
  you change the board target, `--shield` or `-S` on an existing `build/`
  directory, otherwise the previous cached value is kept and your new option is
  silently ignored. Alternatively, give each configuration its own build
  directory with `-d build-<name>`.
- **`--shield zbook_wifi`** adds the ESP8266 Wi-Fi module (an attachable module
  described in the board module at `zbook/boards/shields/zbook_wifi/`) and pulls
  in the networking stack.
- **`-S wifi-credentials`** is an upstream Zephyr snippet that enables
  `WIFI_CREDENTIALS` persistence on the **NVS** settings backend. It uses the
  `storage` flash partition (256 KB), defined by the board.
- **`-S zbook-wifi-credentials-littlefs`** is the equivalent provided by the
  `zbook` board module, but persists credentials via the settings **FILE**
  backend on a **LittleFS** filesystem mounted at `/lfs` on the same `storage`
  partition. Use it *instead of* `-S wifi-credentials` (the two select mutually
  exclusive settings backends). It relies on the `littlefs` module, which is
  already in this app's `west.yml` import allowlist.

### Hardware revisions

Two hardware revisions exist, selected with `@<revision>` on the board target.
**`p1` is the default** — a bare `zbook/rp2350b/m33` builds P1, so P2 hardware
needs the suffix spelled out:

```bash
west build -p always -b zbook/rp2350b/m33            # P1 (default)
west build -p always -b "zbook@p2/rp2350b/m33"       # P2
```

> The default comes from `boards/zbook/revision.cmake`, not from the `default:`
> key in `board.yml`: with `format: custom` Zephyr includes `revision.cmake` and
> ignores that key. To make P2 the default, change `revision.cmake`.

`p1` is the original pinout. `p2` re-pins almost every peripheral and adds the
hall sensor, TMP1075 temperature sensor, PEC12R rotary encoder, DC motor
driver, a second (white) LED, a 4-LED addressable strip and an onboard Wi-Fi
header, and moves the OLED from bit-bang SPI to i2c0. P2 also carries an
onboard RP2040 used purely as a debug probe (SWD + UART bridge); it is not a
Zephyr build target.

| Target                         | Layout                                               |
|--------------------------------|------------------------------------------------------|
| `zbook/rp2350b/m33`            | Standalone, `storage` partition, P1 (default)        |
| `zbook@p2/rp2350b/m33`         | Same layout, P2 hardware                             |
| `zbook/rp2350b/m33/mcuboot`    | MCUboot layout, app in slot-0, P1                    |
| `zbook@p2/rp2350b/m33/mcuboot` | MCUboot layout, app in slot-0, P2                    |

The `zbook_wifi` shield works on both revisions; on P2 it drives the onboard
Wi-Fi header's `WIFI_NRST`/`WIFI_EN` pins (see
`zbook/boards/shields/zbook_wifi/boards/`).

### Board variants and flash layout

The board ships two variants. The partition table is selected by the board
target — there is no run-time/Kconfig switch (devicetree is processed before
Kconfig), so the MCUboot layout lives in a dedicated variant `.dts`.

**`zbook/rp2350b/m33`** — standalone, boots directly from flash:

| Partition       | Offset      | Size      | Notes                          |
|-----------------|-------------|-----------|--------------------------------|
| `image_def`     | `0x000000`  | 256 B     | RP2350 image definition block  |
| `code-partition`| `0x000100`  | ~1.75 MB  | Application (read-only)         |
| `storage`       | `0x1C0000`  | 256 KB    | Settings storage — NVS, or LittleFS with `-S zbook-wifi-credentials-littlefs` |

**`zbook/rp2350b/m33/mcuboot`** — MCUboot layout (upstream
`partitions_2M_sysbuild.dtsi`), application linked into slot-0:

| Partition                 | Offset      | Size    |
|---------------------------|-------------|---------|
| `second_stage_bootloader` | `0x000000`  | 256 B   |
| `mcuboot` (`boot_partition`) | `0x000100` | ~63.5 KB |
| `slot0` (`image-0`)       | `0x010000`  | 832 KB  |
| `slot1` (`image-1`)       | `0x0E0000`  | 832 KB  |
| `storage`                 | `0x1B0000`  | 320 KB  |

> The MCUboot variant only **boots** when MCUboot itself is built and flashed.
> Build it with `--sysbuild` and enable `SB_CONFIG_BOOTLOADER_MCUBOOT=y` (plus a
> signing key). Building the variant without `--sysbuild` only compiles the
> application for slot-0; it will not boot on its own. The standalone target
> remains the one to use for plain bring-up.

### Helper tool (optional)

A `justfile` is provided as a thin convenience wrapper (`just build`,
`just build-wifi`, `just flash`, `just clean`). It is optional — every command
in this document uses `west` directly so nothing depends on `just`.

---

## Flash via OpenOCD

The runner is preconfigured in the board module (`zbook/boards/zbook/board.cmake`)
to use CMSIS-DAP with the `rp2350` target at 4 MHz.

### SWD connection

| Debug Probe | ZBook (RP2350B) |
|-------------|-----------------|
| SWDIO       | GPIO28          |
| SWDCLK      | GPIO29          |
| GND         | GND             |
| 3V3         | 3V3 (optional)  |

### Flash with west

Flashing needs the **Raspberry Pi build of OpenOCD** — it carries the `rp2350`
target support that a stock/system OpenOCD usually lacks, so a plain
`west flash` (which picks up the system `openocd`) fails to find the target.
Point west at the Raspberry Pi `openocd` binary and its script search directory
instead:

```bash
west flash \
  --openocd ~/.local/bin/usr/local/bin/openocd \
  --openocd-search ~/.local/openocd
```

- **`--openocd`** — path to the Raspberry Pi `openocd` binary.
- **`--openocd-search`** — directory holding that OpenOCD's scripts, so the
  `interface/cmsis-dap.cfg` and `target/rp2350.cfg` referenced by the board's
  `board.cmake` resolve.

Adjust both paths to wherever you installed the Raspberry Pi OpenOCD. The
`justfile` wraps this exact invocation — `just flash` — with the paths set at
the top of the file.

### Manual flash with OpenOCD (without west)

```bash
openocd \
  -c "source [find interface/cmsis-dap.cfg]" \
  -c "source [find target/rp2350.cfg]" \
  -c "adapter speed 4000" \
  -c "program build/zephyr/zephyr.elf verify reset exit"
```

---

## Serial monitor

The Zephyr shell is exposed over **UART0** (GPIO29 TX / GPIO28 RX) at
**115200 baud**:

```bash
# Linux / macOS
screen /dev/ttyUSB0 115200
```

After connecting, press **Enter** to bring up the prompt and use `test <module>`
to run a test. With a Wi-Fi build, the `wifi` and `net` shell commands are also
available (e.g. `wifi connect -s <SSID> -p <PASS> -k 1` for a one-shot,
non-persistent connection).

---

## Wi-Fi credentials

Build with a credentials snippet — `-S wifi-credentials` (NVS backend) or
`-S zbook-wifi-credentials-littlefs` (LittleFS backend) — to store Wi-Fi
credentials in the `storage` flash partition so they **survive reboots** and are
**auto-connected on boot** (`CONFIG_WIFI_CREDENTIALS_CONNECT_STORED`). Manage
them from the shell with `wifi cred`:

```text
wifi cred add -s <SSID> -p <PASS> -k 1   # store a network (-k 1 = WPA2-PSK)
wifi cred list                           # list stored networks
wifi cred delete <SSID>                  # remove a stored network (SSID is positional)
wifi cred auto_connect                   # connect now using stored networks
```

After `wifi cred add`, reset the board: the credential is loaded from flash and
the device reconnects on its own — no need to re-enter it.

> On the **first** store on a freshly-formatted LittleFS you'll see a one-time
> `<err> fs: file open error (-2)`: the settings file doesn't exist yet and is
> created by that write. It is harmless and does not reappear on later writes or
> boots.

---

## Repository structure (this repo)

```
bring-up-test-p1/
├── include/                # Test module headers
├── src/                    # Test implementations
│   ├── main.c              #   shell command registration
│   ├── ldr_test.c  io_test.c  pwm_test.c
│   ├── addr_led_test.c  ir_io_test.c
│   └── display_test.c  sd_test.c
├── CMakeLists.txt          # no BOARD_ROOT — board comes from the west module
├── prj.conf                # Application Kconfig (no networking)
└── west.yml                # Manifest: Zephyr + modules + zephyr-book/zbook
```
