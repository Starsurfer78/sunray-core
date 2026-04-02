# BUILD_NOTES.md

## Build Targets

### Linux backend target

| Item | Finding |
| --- | --- |
| Command | `cmake -S . -B <build_dir>` then `cmake --build <build_dir>` |
| Config file | [CMakeLists.txt](/mnt/LappiDaten/Projekte/sunray-core/CMakeLists.txt) |
| Output binary | `<build_dir>/sunray-core` |
| Deployment destination | manual runs in docs use `~/sunray-core/build_pi/sunray-core`; install script uses `ROOT_DIR/build_linux/sunray-core` |
| Confidence | `FACT` |

Evidence:
- [CMakeLists.txt](/mnt/LappiDaten/Projekte/sunray-core/CMakeLists.txt) defines `add_executable(sunray-core main.cpp)`.
- [README.md](/mnt/LappiDaten/Projekte/sunray-core/README.md) shows `build_pi`.
- [install_sunray.sh](/mnt/LappiDaten/Projekte/sunray-core/scripts/install_sunray.sh) defaults to `BUILD_DIR="build_linux"`.

Risk / unknowns:
- `INFERENCE`: `build_pi` is a documented/manual Alfred build directory convention, not a CMake-mandated name.
- `FACT`: no explicit Release-vs-Debug logic is defined in the active CMake files.

### Alfred production build path

| Item | Finding |
| --- | --- |
| Command | documented manual path: `cmake -S . -B build_pi && cmake --build build_pi -j2` |
| Config file | [CMakeLists.txt](/mnt/LappiDaten/Projekte/sunray-core/CMakeLists.txt) |
| Output binary | `build_pi/sunray-core` |
| Deployment destination | run directly on the Pi from repo checkout |
| Confidence | `FACT` for documented path, `INFERENCE` for production preference |

Evidence:
- [ALFRED_TEST_RUN_GUIDE.md](/mnt/LappiDaten/Projekte/sunray-core/docs/ALFRED_TEST_RUN_GUIDE.md)
- [ALFRED_PI_SWITCHOVER_GUIDE.md](/mnt/LappiDaten/Projekte/sunray-core/docs/ALFRED_PI_SWITCHOVER_GUIDE.md)

Risk / unknowns:
- `UNKNOWN`: no packaging step installs the backend binary into `/usr/bin` or another canonical system path.

### Install-script build path

| Item | Finding |
| --- | --- |
| Command | `./scripts/install_sunray.sh` |
| Config file | [install_sunray.sh](/mnt/LappiDaten/Projekte/sunray-core/scripts/install_sunray.sh) |
| Output binary | `build_linux/sunray-core` by default |
| Deployment destination | referenced by generated `systemd` unit as `${ROOT_DIR}/build_linux/sunray-core` |
| Confidence | `FACT` |

Notes:
- Script optionally installs dependencies, creates runtime files, builds WebUI, writes a `systemd` service, and can start it.
- This is the only active repo evidence for an automated productive Pi install path.

### STM32 firmware build path

| Item | Finding |
| --- | --- |
| Command | `bash scripts/flash_alfred.sh build` or `sudo bash scripts/flash_alfred.sh build-flash` |
| Config file | [flash_alfred.sh](/mnt/LappiDaten/Projekte/sunray-core/scripts/flash_alfred.sh), [docs/swd-pi.ocd](/mnt/LappiDaten/Projekte/sunray-core/docs/swd-pi.ocd) |
| Output binary | default `~/sunray_install/firmware/rm18.ino.bin` |
| Deployment destination | flashed to STM32 at address `0x08000000` via OpenOCD |
| Confidence | `FACT` |

Evidence:
- [flash_alfred.sh](/mnt/LappiDaten/Projekte/sunray-core/scripts/flash_alfred.sh)
- [ALFRED_FLASHING.md](/mnt/LappiDaten/Projekte/sunray-core/docs/ALFRED_FLASHING.md)

Important:
- `FACT`: this path uses `arduino-cli`, not PlatformIO.
- `FACT`: firmware source is [rm18.ino](/mnt/LappiDaten/Projekte/sunray-core/alfred/firmware/rm18.ino).

### PlatformIO targets

| Item | Finding |
| --- | --- |
| Active `platformio.ini` | not found in current repo |
| Active `pio run` path | not found in current repo |
| Confidence | `FACT` |

Assessment:
- `FACT`: current repository evidence does not show an active PlatformIO build path.
- `INFERENCE`: `PlatformIO` in [CLAUDE.md](/mnt/LappiDaten/Projekte/sunray-core/CLAUDE.md) is environment context, not a proven active build system in this repo snapshot.

### Raspberry Pi userspace and service build path

| Item | Finding |
| --- | --- |
| Backend command | `cmake -S . -B build_linux` then `cmake --build build_linux` via installer |
| Frontend command | `cd webui-svelte && npm install && npm run build` |
| Config files | [CMakeLists.txt](/mnt/LappiDaten/Projekte/sunray-core/CMakeLists.txt), [package.json](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/package.json) |
| Output artifacts | `build_linux/sunray-core`, `webui-svelte/dist` |
| Deployment destination | repo checkout plus runtime paths under `/etc/sunray-core` and generated `systemd` service |
| Confidence | `FACT` |

### Test binaries

| Item | Finding |
| --- | --- |
| Command | `cmake -S . -B <build_dir>` then `cmake --build <build_dir>`; run via `ctest --test-dir <build_dir>` |
| Config file | [tests/CMakeLists.txt](/mnt/LappiDaten/Projekte/sunray-core/tests/CMakeLists.txt) |
| Output binary | `<build_dir>/tests/sunray_tests` or generator-equivalent |
| Deployment destination | none; local verification artifact |
| Confidence | `FACT` |

### Simulator or debug builds

| Item | Finding |
| --- | --- |
| Simulator runtime command | `./build_pi/sunray-core --sim config.example.json` or installer `--sim` |
| Config file | [main.cpp](/mnt/LappiDaten/Projekte/sunray-core/main.cpp), [install_sunray.sh](/mnt/LappiDaten/Projekte/sunray-core/scripts/install_sunray.sh) |
| Output binary | same `sunray-core` binary |
| Deployment destination | local/dev use; no separate simulator binary |
| Confidence | `FACT` |

### Release vs debug differences

| Item | Finding |
| --- | --- |
| Explicit build type handling | not found in active CMake files |
| Separate release packaging logic | not found |
| Confidence | `FACT` |

Assessment:
- `FACT`: current repo does not define explicit release/debug targets, install layouts, or strip/package steps.
- `UNKNOWN`: productive Alfred units may still use external build flags or wrapper automation outside this repo.

## Config Entry Points

### FACT

- Root backend build entry: [CMakeLists.txt](/mnt/LappiDaten/Projekte/sunray-core/CMakeLists.txt)
- Module build entries:
  - [core/CMakeLists.txt](/mnt/LappiDaten/Projekte/sunray-core/core/CMakeLists.txt)
  - [hal/CMakeLists.txt](/mnt/LappiDaten/Projekte/sunray-core/hal/CMakeLists.txt)
  - [platform/CMakeLists.txt](/mnt/LappiDaten/Projekte/sunray-core/platform/CMakeLists.txt)
  - [tests/CMakeLists.txt](/mnt/LappiDaten/Projekte/sunray-core/tests/CMakeLists.txt)
- Frontend build manifest: [webui-svelte/package.json](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/package.json)
- Runtime config defaults: [core/Config.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Config.cpp)
- Example runtime config: [config.example.json](/mnt/LappiDaten/Projekte/sunray-core/config.example.json)
- STM32 flashing config/script:
  - [scripts/flash_alfred.sh](/mnt/LappiDaten/Projekte/sunray-core/scripts/flash_alfred.sh)
  - [docs/swd-pi.ocd](/mnt/LappiDaten/Projekte/sunray-core/docs/swd-pi.ocd)
- Pi install/service script: [scripts/install_sunray.sh](/mnt/LappiDaten/Projekte/sunray-core/scripts/install_sunray.sh)
- Hardware/runtime check scripts:
  - [scripts/check_alfred_hw.sh](/mnt/LappiDaten/Projekte/sunray-core/scripts/check_alfred_hw.sh)
  - [scripts/check_rm18_uart.sh](/mnt/LappiDaten/Projekte/sunray-core/scripts/check_rm18_uart.sh)

### Environment variables

| Variable | Source | Classification |
| --- | --- | --- |
| `CONFIG_PATH` | `main.cpp` default config override | `FACT` |
| `OPENOCD_BIN`, `OPENOCD_CFG` | `flash_alfred.sh` | `FACT` |
| `ARDUINO_CLI_BIN`, `FQBN` | `flash_alfred.sh` | `FACT` |
| `SUNRAY_INSTALL_DIR`, `ALFRED_SKETCH`, `BUILD_DIR`, `BIN_PATH` | `flash_alfred.sh` | `FACT` |
| `CONFIG_PATH`, `DRIVER_PORT`, `I2C_BUS` | hardware/UART check scripts | `FACT` |

### systemd service files

| Service path | Source | Classification |
| --- | --- | --- |
| `/etc/systemd/system/sunray-core.service` | dynamically generated by `install_sunray.sh` | `FACT` |
| Existing original service `sunray` | referenced in switchover and test guides, not stored as file in this repo | `FACT` for existence in docs, `UNKNOWN` for exact content on target |

### Startup shell scripts

| Script | Purpose | Classification |
| --- | --- | --- |
| `scripts/install_sunray.sh` | build/install/start service on Pi | `FACT` |
| `scripts/flash_alfred.sh` | build/flash STM32 firmware | `FACT` |
| `scripts/check_alfred_hw.sh` | preflight hardware checks on Pi | `FACT` |
| `scripts/check_rm18_uart.sh` | UART protocol check against RM18 | `FACT` |

### Board-specific overrides

| Item | Finding | Classification |
| --- | --- | --- |
| Alfred-specific runtime defaults like `driver_port`, mux channels, charger threshold | active in `Config.cpp` and docs | `FACT` |
| Productive config at `/etc/sunray-core/config.json` | installer/docs path | `FACT` |
| Local hardware-specific overrides in target config | expected by docs but not versioned in repo | `FACT` for expectation, `UNKNOWN` for exact deployed values |
| `config.example.json` as canonical shipped baseline for active Alfred defaults | repo example plus config regression test | `FACT` |

## Startup Order

### systemd-managed startup path from repo evidence

1. `systemd` starts `sunray-core.service` after `network-online.target`.
   Source: `install_sunray.sh` generated unit.
   Classification: `FACT`
2. `ExecStart` launches `${ROOT_DIR}/${BUILD_DIR}/sunray-core` with either:
   - hardware mode: `/etc/sunray-core/config.json`
   - sim mode: `--sim config.example.json`
   Classification: `FACT`
3. `main.cpp` resolves config path, with precedence:
   - CLI path
   - `CONFIG_PATH`
   - `/etc/sunray-core/config.json`
   - fallback `/etc/sunray/config.json`
   Classification: `FACT`
4. Backend constructs `Config`, logger, driver, and `Robot`.
   Classification: `FACT`
5. `robot.init()` runs hardware initialization first.
   If this fails, process exits with failure.
   Classification: `FACT`
6. In non-sim mode, GPS driver is initialized after `robot.init()`.
   If GPS init fails, runtime continues without GPS.
   Classification: `FACT`
7. `WebSocketServer` is constructed, configured, and started.
   It binds to `0.0.0.0` on config key `ws_port` default `8765`.
   Classification: `FACT`
8. `MqttClient` is constructed and `start()` is called after WebSocket startup.
   If `mqtt_enabled=false`, it logs disabled and returns.
   If libmosquitto was unavailable at build time, it logs a warning and remains a stub.
   Classification: `FACT`
9. `robot.loop()` starts and becomes the long-running runtime heartbeat.
   Classification: `FACT`
10. On shutdown, MQTT stops first, then WebSocket stops.
    Classification: `FACT`

### Failure behavior in startup order

| Missing or failed component | Observed behavior | Classification |
| --- | --- | --- |
| hardware init | process exits before serving UI | `FACT` |
| GPS driver init | warning, continue without GPS | `FACT` |
| MQTT disabled | continue normally | `FACT` |
| MQTT library missing at build time | warning, continue with no-op MQTT | `FACT` |
| WebUI `dist` not found | server falls back to relative path resolution; exact startup failure behavior is not proven here | `UNKNOWN` |
| missing network | `systemd` unit wants `network-online.target`, but exact broker/GPS/network race behavior beyond that is not fully documented | `INFERENCE` |

### Dependency order assessment

- Pi ↔ STM32 init order:
  - `FACT`: Pi opens UART/I2C during `robot.init()` through `SerialRobotDriver::init()`.
- MQTT startup dependency:
  - `FACT`: MQTT starts after WebSocket and after `robot.init()`.
  - `FACT`: runtime does not require MQTT to proceed.
- RTK dependency order:
  - `FACT`: GPS driver attaches after `robot.init()`.
  - `FACT`: runtime can proceed without GPS.
- HA/API startup order:
  - `FACT`: HTTP/WebSocket API starts before MQTT.
  - `INFERENCE`: Home Assistant discovery depends on MQTT being enabled and connected, not on core startup success.

## Deployment Flow

### Pi runtime deployment

#### Manual Alfred test path

1. Update repo on Pi: `git pull origin master`.
2. Build backend: `cmake -S . -B build_pi && cmake --build build_pi -j2`.
3. Build WebUI: `cd webui-svelte && npm install && npm run build`.
4. Ensure runtime files under `/etc/sunray-core`.
5. Stop original service: `sudo systemctl stop sunray`.
6. Start `./build_pi/sunray-core /etc/sunray-core/config.json` in foreground.
7. If bad, rollback with `sudo systemctl start sunray`.

Classification: `FACT`

Sources:
- [ALFRED_TEST_RUN_GUIDE.md](/mnt/LappiDaten/Projekte/sunray-core/docs/ALFRED_TEST_RUN_GUIDE.md)
- [ALFRED_PI_SWITCHOVER_GUIDE.md](/mnt/LappiDaten/Projekte/sunray-core/docs/ALFRED_PI_SWITCHOVER_GUIDE.md)

#### Installer-driven path

1. Run `./scripts/install_sunray.sh`.
2. Optionally install apt dependencies.
3. Ensure `/etc/sunray-core/config.json` and `/etc/sunray-core/map.json` exist.
4. Build backend into `build_linux`.
5. Build WebUI into `webui-svelte/dist`.
6. Optionally write `/etc/systemd/system/sunray-core.service`.
7. Optionally enable and restart that service.

Classification: `FACT`

#### Deployment verification path

1. Verify repo-side artefacts and service anchors with `bash scripts/check_deploy_state.sh`.
2. Verify hardware-side prerequisites with `bash scripts/check_alfred_hw.sh`.
3. Verify STM32 UART responsiveness with `bash scripts/check_rm18_uart.sh`.
4. Only then switch from manual foreground testing to `systemd`-managed startup.

Classification: `FACT`

Evidence:
- [scripts/check_deploy_state.sh](/mnt/LappiDaten/Projekte/sunray-core/scripts/check_deploy_state.sh)
- [scripts/check_alfred_hw.sh](/mnt/LappiDaten/Projekte/sunray-core/scripts/check_alfred_hw.sh)
- [scripts/check_rm18_uart.sh](/mnt/LappiDaten/Projekte/sunray-core/scripts/check_rm18_uart.sh)

### Firmware flashing path

1. Provide `rm18.ino` under `~/sunray_install/alfred/firmware/rm18.ino` or override via env.
2. Install `arduino-cli` and STM32 board support.
3. Set `FQBN=STMicroelectronics:stm32:GenF1:pnum=GENERIC_F103VETX`.
4. Compile to `~/sunray_install/firmware/rm18.ino.bin`.
5. Probe SWD via OpenOCD.
6. Flash to STM32 at `0x08000000`.

Classification: `FACT`

### Config deployment

| File | Path | Evidence | Classification |
| --- | --- | --- | --- |
| runtime config | `/etc/sunray-core/config.json` | installer/docs/main fallback | `FACT` |
| map | `/etc/sunray-core/map.json` | installer/docs/config defaults | `FACT` |
| missions | `/etc/sunray-core/missions.json` | config default path | `FACT` |
| schedule | `<config_dir>/schedule.json` | `main.cpp` schedule setup | `FACT` |
| history DB | `/var/lib/sunray/history.db` by default | `Config.cpp`, `HistoryDatabase.cpp` | `FACT` |

### Remote deployment assumptions

- `FACT`: docs assume SSH access to the Pi.
- `FACT`: docs assume repo checkout under `~/sunray-core`.
- `UNKNOWN`: no active rsync, scp, package-manager, or image-based deployment tool is present in this repo.

### OTA / update capability

- `FACT`: no OTA/update subsystem was found in active repo code or scripts.
- `UNKNOWN`: any external fleet or imaging mechanism outside this repo.

## Rollback & Recovery

### Safe rollback anchors found in repo evidence

| Situation | Recovery path | Classification |
| --- | --- | --- |
| `sunray-core` manual startup fails | restart original `sunray` service | `FACT` |
| first Alfred switchover test | docs explicitly require keeping original service intact and not switching autostart yet | `FACT` |
| bad config overwrite risk | docs warn that copying `config.example.json` can destroy Alfred-specific values | `FACT` |
| `sunray-core.service` bad install | `systemd` unit is generated locally and can be disabled/removed, but no scripted rollback helper is provided | `FACT` for generation, `UNKNOWN` for standardized rollback workflow |

### If STM32 flash fails

- `FACT`: `flash_alfred.sh` runs `probe` before flash in `flash` and `build-flash` modes.
- `FACT`: script exits on error because of `set -euo pipefail`.
- `FACT`: no explicit backup/readback of existing STM32 firmware is implemented in the repo scripts.
- `UNKNOWN`: board state after partial or failed flash.
- `UNKNOWN`: documented field recovery path if the MCU no longer boots after a bad flash.

### If Pi service fails after reboot

- `FACT`: generated `sunray-core.service` uses `Restart=on-failure` and `RestartSec=5`.
- `FACT`: it starts only after `network-online.target`.
- `UNKNOWN`: whether that is sufficient for all productive dependencies.
- `UNKNOWN`: whether the original `sunray` service remains enabled when `sunray-core.service` is enabled on a real unit.

### Manual recovery workflow evidenced in docs

1. Stop testing process or let it fail.
2. Start original service: `sudo systemctl start sunray`.
3. Review logs and retry later.

Classification: `FACT`

### Verifiable recovery checklist

1. Run `bash scripts/check_deploy_state.sh` and confirm whether:
   - a `sunray-core` binary exists in `build_linux` or `build_pi`
   - `/etc/sunray-core/config.json` and `/etc/sunray-core/map.json` exist
   - `/etc/systemd/system/sunray-core.service` points to a real binary
   - the original `sunray` service is still visible as rollback anchor
2. If foreground test or `sunray-core.service` fails, stop it explicitly:
   - `sudo systemctl stop sunray-core`
3. Restore the original productive service:
   - `sudo systemctl start sunray`
4. Compare logs before another deploy attempt:
   - `journalctl -u sunray-core -n 100 --no-pager`
   - `journalctl -u sunray -n 100 --no-pager`

Classification: `FACT` for commands and script behavior, `INFERENCE` for recommended order.

### Watchdog interaction during bad deploy

- `FACT`: STM32 has an internal watchdog and motor command timeout.
- `FACT`: Pi-side service script does not configure a `systemd` watchdog.
- `INFERENCE`: a broken Pi deployment may still leave MCU-side timeouts as the only proven last-resort motion fallback.

## Open Unknowns

- `UNKNOWN`: active production `systemd` service content for the original `sunray` runtime
- `UNKNOWN`: whether productive Alfred units actually use `build_pi`, `build_linux`, or another build directory convention
- `UNKNOWN`: external packaging or image-building pipeline
- `UNKNOWN`: any rollback path for STM32 firmware beyond reflash
- `UNKNOWN`: whether `network-online.target` is enough for broker, RTK/NTRIP, and other productive dependencies
- `UNKNOWN`: exact startup race behavior if WebSocket port bind fails or web assets are missing
- `UNKNOWN`: exact deployment destination for frontend assets outside repo checkout
- `UNKNOWN`: standardized safe rollback if `sunray-core.service` replaces the original service in the field
- `FACT`: no active PlatformIO config or target was found in this repo snapshot
- `FACT`: repo now contains a read-only deployment verifier in `scripts/check_deploy_state.sh`, but it does not perform rollback automatically

## FACT / INFERENCE / UNKNOWN Summary

### FACT

- Active backend build system is CMake.
- Active frontend build system is npm/Vite/Svelte.
- Productive Pi install path in this repo is `scripts/install_sunray.sh`.
- Productive manual Alfred test path in docs uses `build_pi` and foreground startup.
- Runtime config path is `/etc/sunray-core/config.json` unless overridden.
- Generated `systemd` service is `sunray-core.service` with `After=Wants=network-online.target`.
- `main.cpp` startup order is: config -> logger -> hardware driver -> `Robot` -> `robot.init()` -> GPS -> WebSocket -> MQTT -> `robot.loop()`.
- STM32 firmware flashing uses `arduino-cli` plus OpenOCD, not PlatformIO.

### INFERENCE

- The documented safe field path is staged switchover: keep original `sunray` as rollback anchor, test `sunray-core` manually first, only later consider autostart migration.
- MQTT and HA are additive runtime integrations, not hard startup prerequisites.
- Missing network may affect broker or RTK behavior even though the service waits for `network-online.target`.

### UNKNOWN

- No fully proven end-to-end productive deployment chain beyond repo checkout on the Pi
- No proven OTA/update system
- No proven backup/rollback mechanism for STM32 firmware images
- No proven external supervisor beyond generated `systemd` restart behavior
