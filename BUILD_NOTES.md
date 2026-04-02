# Build Notes

## Backend Build Targets

- Main executable: `sunray-core`
- Test executable: `sunray_tests`
- Probe tools:
  - `rm18_serial_probe`
  - `ex2_buzzer_probe`
  - `ex3_led_probe`
  - `ex_led_buzzer_test`
  - `pca9555_probe`

## Build System

- Backend: CMake, C++17
- Frontend: Node, Vite, Svelte
- Current repo uses `FetchContent` for at least `nlohmann_json`, `Catch2`, `asio`, and `Crow`

## Typical Local Build

### Backend

```bash
cmake -S . -B build_verify
cmake --build build_verify -j4
ctest --test-dir build_verify --output-on-failure
```

### Frontend

```bash
cd webui-svelte
npm install
npm run build
```

## Linux And Alfred-Relevant Paths

- Repo root: `/mnt/LappiDaten/Projekte/sunray-core`
- Default serial driver port: `/dev/ttyS0`
- Default I2C bus: `/dev/i2c-1`
- Default GPS path: `/dev/serial/by-id/...u-blox...`

## Config Headers And Runtime Config

- Compile-time structure is in code plus CMake
- Runtime defaults live mainly in `core/Config.cpp`
- Example config artifacts include `config.example.json` and `config_examples.txt`

## Deployment

### FACT

- The Pi-side runtime is intended for Alfred Linux deployment
- The repo contains docs for flashing, hardware acceptance, switchover, and test runs under `docs/`

### INFERENCE

- Deployment likely means updating the Pi binary, config, and optionally frontend assets together

### UNKNOWN

- Exact production service names
- Whether deployment is manual, scripted, or image-based

## Startup Services

### FACT

- No active `.service` unit files were found in the shallow repo scan performed for this bootstrap

### UNKNOWN

- Exact `systemd` units and ordering for backend, networking, MQTT bridge, or frontend hosting

## Update Path

- Rebuild backend
- Rebuild frontend if UI changed
- Deploy binary, config, and assets in controlled order
- Re-run acceptance or smoke checks before field use

## Rollback And Recovery Considerations

- Keep a known-good backend binary available
- Keep prior config versions recoverable
- Avoid mixed backend/frontend deployments with incompatible telemetry schemas
- Validate dock, battery, and stop behavior after any runtime change

## Open Unknowns

- Production deploy script
- Service supervision tree
- On-device log locations and retention policy
