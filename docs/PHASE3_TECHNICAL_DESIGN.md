# Phase 3 Technical Design — Differentiators

**Autoren-Klassifizierung:** Mammotion vs Segway vs sunray-core Competitive Advantage

---

## 1. OTA Updates — Secure Remote Firmware Deployment

### 1.1 System Architecture

```
┌──────────────┐         ┌──────────────────┐         ┌─────────────┐
│ Update Server│         │  Raspberry Pi    │         │   STM32     │
│              │         │  (Core + WebUI)  │         │   Alfred    │
└──────┬───────┘         └────────┬─────────┘         └──────┬──────┘
       │                          │                          │
   (1) Request                    │                          │
   /api/ota/check                 │                          │
       │                   (2) Download Binary               │
       ├─────────────────────────────────────┐              │
       │                                      │              │
       │ (3) Verify Signature (Ed25519)       │              │
       │ (4) Hash Check + Stored to /tmp     │              │
       │                                      │              │
       │ (5) Invoke OTA Manager              │              │
       │     - Backup current to slot_B      │              │
       │     - Write new to slot_A           │              │
       │     - Update Bootloader Flag        │              │
       │                            (6) Reboot
       │                                      ├──────────────┤
       │                                      │  (7) Boot    │
       │                                      │  Slot A      │
       │                                      │  (Health)    │
       │                                      │──────────────┤
       │                                      │ (8) Report OK
       │                                      │
       │ (9) Cleanup old binary              │
       │                                      │
       └──────────────────────────────────────┘
```

### 1.2 File Structure

```
sunray-core/
  core/ota/
    ├── OtaManager.h
    ├── OtaManager.cpp              # Download + verify + write
    ├── OtaConfig.h                 # Slot layout, keys, timeouts
    └── OtaBootloader.h             # Dual-slot + rollback logic

  hal/
    ├── Bootloader/
    │   ├── STM32F103Bootloader.cpp # Dual-slot for STM32
    │   └── PicoBootloader.cpp      # Dual-slot for RP2040

  core/
    ├── WebSocketServer.cpp         # Add /api/ota/* endpoints
    └── Robot.h                     # periodicOtaCheck() call

  tests/
    ├── test_ota_manager.cpp        # Signature verify, corruption
    ├── test_ota_rollback.cpp       # Failed update recovery
```

### 1.3 Data Structures

```cpp
// core/ota/OtaConfig.h
struct OtaSlotInfo {
    uint32_t address;              // Flash address (0x08000000 or 0x08040000)
    uint32_t size;                 // Max size (256KB for STM32F103)
    uint32_t crc32;                // Current CRC
    uint32_t version;              // Firmware version
    bool     active;               // Currently booting from this slot
};

struct OtaManifest {
    uint32_t version;              // New firmware version
    uint32_t size;                 // Binary size
    uint8_t  sha256[32];           // SHA-256 of binary
    uint8_t  ed25519_sig[64];      // Signature (64 bytes)
    char     changelog[256];       // Plain text: "- Fix bumper debounce\n- Add zone reordering"
};

// core/ota/OtaManager.h
class OtaManager {
public:
    bool checkForUpdates();        // HTTP GET /api/releases/latest
    bool downloadBinary(const std::string& url);
    bool verifySecurity();         // Ed25519 verify + SHA-256 check
    bool stageUpdate();            // Write to inactive slot
    bool commitUpdate();           // Mark as active, reboot
    bool rollbackOnFail();         // If health check fails, boot old slot
};
```

### 1.4 Config Keys

```json
{
  "_c_ota": "Over-The-Air firmware updates",
  "ota_enabled": true,
  "ota_check_interval_hours": 24,
  "ota_server_url": "https://releases.sunray.local/api/",
  "ota_require_approved": false,  // true = manual confirmation before install
  "ota_rollback_on_error_count": 5,  // revert if failed N times
  "ota_ed25519_pubkey": "..."
}
```

### 1.5 WebSocket Endpoints

```
GET /api/ota/status
  → { "current_version": "2.1.4", "last_check": 1711270400, "update_available": true }

GET /api/ota/latest
  → { "version": "2.2.0", "changelog": "...", "size_mb": 2.1, "release_date": "2026-04-01" }

POST /api/ota/download {"version": "2.2.0"}
  → Streams binary, reports progress { "percent": 45, "eta_s": 60 }

POST /api/ota/apply
  → Verifies, stages, commits. Returns { "status": "rebooting", "eta_s": 30 }

POST /api/ota/rollback
  → Falls back to previous slot (emergency)
```

### 1.6 Security Considerations

| Threat | Mitigation |
|--------|-----------|
| **MITM Attack** | Ed25519 signature (64 bytes), verified before any write |
| **Corrupted Binary** | SHA-256 hash check + CRC-32 after flash |
| **Bricked Device** | Dual-slot with fallback (if boot fails, use old slot) |
| **Incomplete Download** | Ranges header support, resume capability |
| **Supply Chain** | Keypair in Release Build, not version control |
| **Downgrade Attack** | Version counter (cannot downgrade to older) |

### 1.7 Testing Strategy

```cpp
// tests/test_ota_manager.cpp
TEST("[ota] corrupt signature rejected") {
  auto mgr = OtaManager(...);
  Manifest corruptSig = validManifest;
  corruptSig.ed25519_sig[0] ^= 0xFF;  // flip bit
  REQUIRE_FALSE(mgr.verifySecurity(corruptSig));
}

TEST("[ota] rollback on failed health check") {
  // Simulate boot into new slot, then health check fails
  // Bootloader should see error flag, revert to old slot
}
```

---

## 2. Sensor Fusion — Kalman Filter for Position & Heading

### 2.1 StateEstimator Redesign (Phase 2.5 refactor)

```cpp
// core/navigation/StateEstimator.h (REDESIGNED for P3)

class StateEstimator {
    // Kalman Filter state: [x, y, vx, vy, heading, omega]ᵀ
    // Measurement: [encoder_x, encoder_y, gps_x, gps_y, gyro_heading]

    struct KalmanState {
        Eigen::Vector6f state;           // [x, y, vx, vy, heading, omega]
        Eigen::Matrix6f covariance;      // Σ (uncertainty)
        Eigen::Matrix6f Q;               // Process noise (config)
        Eigen::Matrix5x6f H;             // Measurement matrix
        Eigen::Matrix<float, 5, 5> R;    // Measurement noise (config)
    };

    KalmanState kf_;

    void predict(float dt_s);           // Time update
    void updateOdometry(OdometryData);  // Measurement update (encoder)
    void updateGps(GpsData);            // Measurement update (GPS)
    void updateGyro(float heading_rad); // Measurement update (optional IMU)

    float getInnovation() const;        // Mahalanobis distance (outlier detection)
};
```

### 2.2 Config Keys for Fusion

```json
{
  "_c_fusion": "Sensor fusion via Kalman Filter",
  "fusion_enabled": true,
  "fusion_filter_type": "ukf",          // "ekf" or "ukf"
  "fusion_process_noise_q": 0.01,       // odometry noise (m²/s²)
  "fusion_measurement_noise_r": 0.05,   // GPS noise (m²)
  "fusion_gps_outlier_threshold": 3.0,  // Mahalanobis (reject if >3σ)
  "fusion_headfirst_mode": true,        // use gyro for heading before GPS
  "fusion_diagnostics": true            // log innovation, covariance
}
```

### 2.3 Failure Modes & Recovery

```
GPS Signal Lost:
  1. Innovation grows beyond threshold
  2. Filter goes to Odometry-only (R → ∞ for GPS)
  3. Uncertainty increases, but steering continues
  4. On GPS reacquisition: smooth re-integration (no jump)

Encoder Jam (detected by >0.5m/frame sanity guard):
  1. Encoder measurement rejected
  2. Filter falls back to GPS-only (if available)
  3. Heading from gyro (if available) or last known

Gyro Drift (if IMU added):
  1. Heading diverges from GPS track
  2. GPS corrects, gyro recalibrated
  3. Long-term stability from GPS, short-term from gyro
```

### 2.4 Output & Diagnostics

```json
// WebSocket telemetry extension (P3)
{
  "type": "state",
  ...,
  "_fusion": {
    "filter_mode": "gps_odo",           // "gps_only" | "odo_only" | "gps_odo" | "error"
    "position_uncertainty_m": 0.12,     // σ from covariance
    "heading_uncertainty_rad": 0.05,
    "innovation": 0.8,                  // Mahalanobis distance (debug)
    "gps_age_ms": 150,                  // How fresh is GPS measurement?
    "sensor_health": ["odo_ok", "gps_float", "gyro_available"]
  }
}
```

---

## 3. Adaptive Learning — TensorFlow Lite Online Training

### 3.1 Data Flow

```
Session Start → Collect Metrics → Session End
    ↓                                   ↓
  [Performance Data]    ┌─────────────────────┐
  - Zone size (m²)      │  Store in Config:   │
  - Soil moisture (%)   │  sessions[] history │
  - Pattern (angle)     │  (last 30 sessions) │
  - Speed (m/s)         └─────────────────────┘
  - Time (s)                    ↓
  - Obstacles (#)        Every 5th session:
  - Battery drain (%)    ┌──────────────┐
  - Weather (temp)       │  Retrain ML  │
                         │  Model on Pi │
                         │  (offline)   │
                         └──────┬───────┘
                                ↓
                        ┌─────────────────────┐
                        │ New Coefficients:   │
                        │ speed_factor[zone] │
                        │ pattern[zone]      │
                        │ energy_budget[soil]│
                        └──────────┬──────────┘
                                   ↓
                        [Apply to next session]
```

### 3.2 TensorFlow Lite Model

```cpp
// core/learning/AdaptiveModel.h

class AdaptiveModel {
    // 4 inputs → 3 outputs
    // Inputs:  [zone_area_m2, soil_moisture%, prev_speed_ms, obstacle_density]
    // Outputs: [speed_factor, pattern_angle, urgency_score]

    // Model: 2 hidden layers (32 neurons each), ReLU
    tflite::Interpreter* interpreter_;
    tflite::ModelT* model_;

    std::vector<float> predict(const SessionMetrics& session);
    void train(const std::vector<SessionMetrics>& history);  // Offline
};
```

**Model Size:** ~80 KB (TFLite quantized int8)
**Inference Time:** 3–5 ms (Pi 4B)
**Training Time:** ~2 min per epoch (offline, non-blocking)

### 3.3 Session Metrics Storage

```cpp
// core/learning/SessionMetrics.h

struct SessionMetrics {
    uint64_t timestamp_ms;

    // Environment
    uint32_t zone_area_m2;
    uint16_t soil_moisture_pct;    // 0-100 (user input or sensor)
    uint8_t  weather_code;         // 0=sunny, 1=cloudy, 2=rain, 3=wet

    // Performance
    float    avg_speed_ms;
    uint32_t total_time_s;
    float    energy_consumed_pct;  // battery end - start
    uint16_t obstacle_count;
    uint8_t  pattern_angle_deg;

    // Outcome
    bool     completed;
    uint8_t  efficiency_score;     // 0-100 (area/time/energy)
};
```

**Storage:** Base64-encoded vector in `config.json`
```json
{
  "learning_session_history": "gYAMAAAAhwAXAA8AAAADAAAAOAAAACgAKgADABMABw==",
  "learning_model": "AgAMAAAA+..."
}
```

### 3.4 Training Pipeline (Python Helper)

```python
# tools/train_model.py

import numpy as np
import tensorflow as tf
import json
import base64

# Load sessions from config
with open('config.json') as f:
    cfg = json.load(f)
    sessions = deserialize_sessions(base64.b64decode(cfg['learning_session_history']))

# Prepare training data
X = np.array([[s.zone_area, s.soil_pct, s.avg_speed, s.obstacles] for s in sessions])
y = np.array([[s.speed_factor, s.pattern_angle, s.efficiency] for s in sessions])

# Build & train model
model = tf.keras.Sequential([
    tf.keras.layers.Dense(32, activation='relu', input_shape=(4,)),
    tf.keras.layers.Dense(32, activation='relu'),
    tf.keras.layers.Dense(3)  # 3 outputs
])
model.compile(optimizer='adam', loss='mse')
model.fit(X, y, epochs=50, verbose=0)

# Quantize & export
converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.target_spec.supported_ops = [
    tf.lite.OpsSet.TFLITE_BUILTINS_INT8
]
tflite_model = converter.convert()

# Store back to config
cfg['learning_model'] = base64.b64encode(tflite_model).decode()
with open('config.json', 'w') as f:
    json.dump(cfg, f)
print("Model retrained and saved to config.json")
```

### 3.5 Validation & A/B Testing

```cpp
// MowOp::run() — A/B test different strategies

void MowOp::run(OpContext& ctx) {
    // Decide: use learned or legacy?
    float speed_target = ctx.map->currentZone().speed;

    if (ab_test_enabled && session_count_ % 10 == 0) {
        // Alternating: test A (legacy) vs B (ML-optimized)
        if (session_count_ % 20 == 0) {
            // Variant B: ML-predicted speed
            float speed_factor = adaptive_model_->predict(current_metrics_)[0];
            speed_target *= speed_factor;
            ctx.logger.info("MowOp", "A/B Test B (ML-optimized): " + std::to_string(speed_target));
        } else {
            ctx.logger.info("MowOp", "A/B Test A (baseline): " + std::to_string(speed_target));
        }
    }

    ctx.setLinearAngularSpeed(speed_target, angular_correction);
}
```

**Dashboard Display:**
```
Mowing Efficiency over 10 sessions:
  Session 1-5:  Baseline 100%
  Session 6-10: ML-Optimized 125%

  → 25% faster after learning!

  Trend: ┌─────
         │      /
         │   /
         │ /
         └─────
```

---

## 4. Mobile Native App — iOS + Android

### 4.1 Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                   Mobile Devices                            │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              iOS (Swift + SwiftUI)                   │   │
│  │  ┌────────────────┐  ┌──────────────────────────┐    │   │
│  │  │  Dashboard     │  │  Map (Mapbox GL Native) │    │   │
│  │  │  - Status      │  │  - Live Robot Position  │    │   │
│  │  │  - Battery %   │  │  - Waypoints            │    │   │
│  │  │  - Session     │  │  - Zones                │    │   │
│  │  └────────────────┘  └──────────────────────────┘    │   │
│  │  ┌──────────────────────────────────────────────┐    │   │
│  │  │  Zone Editor (Offline-capable)               │    │   │
│  │  │  - Draw perimeter, exclusions, zones         │    │   │
│  │  │  - Mow path preview (cached)                 │    │   │
│  │  │  - Settings per zone                         │    │   │
│  │  └──────────────────────────────────────────────┘    │   │
│  │  ┌──────────────────────────────────────────────┐    │   │
│  │  │  SQLite Local Database                       │    │   │
│  │  │  - Map cache                                 │    │   │
│  │  │  - Session history                           │    │   │
│  │  │  - Settings snapshot                         │    │   │
│  │  └──────────────────────────────────────────────┘    │   │
│  └──────────────────────────────────────────────────────┘   │
│         ↓ WebSocket                                        │
├─────────┼──────────────────────────────────────────────────┤
│  Raspberry Pi: sunray-core (already exists)                │
│  WebSocket /ws/telemetry 10 Hz                             │
│  REST /api/map, /api/config, /api/ota, etc.               │
└─────────┼──────────────────────────────────────────────────┘
          │
    ┌─────▼──────┐
    │  STM32 MCU │
    └────────────┘
```

### 4.2 iOS Implementation (Swift)

**File Structure:**
```
sunray-ios/
  ├── SunrayApp.swift              # Entry point
  ├── Views/
  │   ├── DashboardView.swift      # Home screen
  │   ├── MapView.swift            # Mapbox GL integration
  │   ├── ZoneEditorView.swift     # Zone drawing
  │   ├── SettingsView.swift       # Config, OTA, updates
  │   └── HistoryView.swift        # Session log
  ├── Models/
  │   ├── Robot.swift              # @Published state
  │   ├── Map.swift
  │   ├── Session.swift
  │   └── Settings.swift
  ├── Services/
  │   ├── WebSocketService.swift   # Connection + reconnect
  │   ├── DatabaseService.swift    # SQLite
  │   ├── LocationService.swift    # Background location
  │   └── NotificationService.swift # Push alerts
  └── Resources/
      └── Assets.xcassets
```

**Key Code Snippet:**
```swift
// Services/WebSocketService.swift

import Foundation
import Combine

class WebSocketService: NSObject, URLSessionWebSocketDelegate, ObservableObject {
    @Published var telemetry: TelemetryData?
    @Published var isConnected = false

    var webSocket: URLSessionWebSocket?
    var reconnectTimer: Timer?

    func connect(url: URL) {
        let request = URLRequest(url: url)
        webSocket = URLSession.shared.webSocketTask(with: request)
        webSocket?.resume()
        isConnected = true
        receiveMessage()
    }

    func receiveMessage() {
        webSocket?.receive { [weak self] result in
            switch result {
            case .success(let message):
                switch message {
                case .string(let text):
                    if let data = text.data(using: .utf8),
                       let json = try? JSONDecoder().decode(TelemetryData.self, from: data) {
                        DispatchQueue.main.async {
                            self?.telemetry = json
                        }
                    }
                case .data(let data):
                    break
                @unknown default:
                    break
                }
                self?.receiveMessage()
            case .failure:
                self?.handleDisconnection()
            }
        }
    }

    func handleDisconnection() {
        DispatchQueue.main.async {
            self.isConnected = false
        }
        reconnectTimer = Timer.scheduledTimer(withTimeInterval: 3, repeats: false) { [weak self] _ in
            self?.connect(url: /* stored URL */)
        }
    }
}
```

### 4.3 Android Implementation (Kotlin)

**File Structure:**
```
sunray-android/
  ├── MainActivity.kt
  ├── ui/
  │   ├── dashboard/
  │   │   ├── DashboardScreen.kt
  │   │   └── DashboardViewModel.kt
  │   ├── map/
  │   │   ├── MapScreen.kt (Mapbox GL)
  │   │   └── MapViewModel.kt
  │   └── settings/
  │       └── SettingsScreen.kt
  ├── data/
  │   ├── room/
  │   │   ├── MapDatabase.kt
  │   │   └── SessionDao.kt
  │   └── remote/
  │       ├── WebSocketClient.kt
  │       └── ApiClient.kt (Retrofit)
  ├── domain/
  │   ├── Robot.kt (Data class)
  │   └── UseCases.kt
  └── MainActivity.kt
```

### 4.4 Shared Features

| Feature | iOS | Android | Sync |
|---------|-----|---------|------|
| **Real-time Telemetry** | WebSocket | WebSocket | Same |
| **Offline Map Cache** | CoreData | Room DB | SQLite compatible |
| **Geofencing** | CLCircularRegion | Geofence API | App-level logic |
| **Location Tracking** | Background Mode | WorkManager | LocalBroadcast |
| **Push Notifications** | APNs | FCM | MQTT bridge |
| **Map Editor** | SwiftUI Canvas | Jetpack Canvas | Mapbox GL Native |

### 4.5 Mapbox GL Native Integration

```swift
// iOS: MapboxMaps (native, not web)
import MapboxMaps

struct MapView: UIViewRepresentable {
    func makeUIView(context: Context) -> MapView {
        var mapOptions = MapInitOptions()
        mapOptions.cameraOptions = CameraOptions(
            center: CLLocationCoordinate2D(latitude: 51.23, longitude: 6.78),
            zoom: 15
        )
        let mapboxMap = MapView(frame: .zero, mapInitOptions: mapOptions)

        // Add perimeter, zones, robot marker
        addPerimeterAnnotation(to: mapboxMap)
        addRobotMarker(to: mapboxMap)

        return mapboxMap
    }
}
```

**Performance:** 60fps on iPhone 12, 120fps on iPad Pro
**Offline:** Cached map tiles (pre-downloaded style)

### 4.6 Database Schema (Room/CoreData)

```sql
-- AndroidAppDB.db

CREATE TABLE robot_sessions (
    id INTEGER PRIMARY KEY,
    timestamp_ms INTEGER,
    zone_id TEXT,
    duration_s INTEGER,
    distance_m REAL,
    efficiency_score INTEGER
);

CREATE TABLE cached_maps (
    id TEXT PRIMARY KEY,
    geojson TEXT,
    downloaded_at INTEGER,
    expires_at INTEGER
);

CREATE TABLE settings_snapshot (
    id INTEGER PRIMARY KEY,
    config_json TEXT,
    sync_timestamp_ms INTEGER
);
```

### 4.7 Testing Strategy

```swift
// Tests/WebSocketServiceTests.swift

@testable import Sunray

class WebSocketServiceTests: XCTestCase {
    var sut: WebSocketService!
    var mockURL: URL!

    func testReconnectAfterDisconnection() {
        // Simulate WS disconnect
        sut.handleDisconnection()

        // Verify reconnect timer started
        XCTAssertNotNil(sut.reconnectTimer)

        // Advance time
        RunLoop.main.run(until: Date().addingTimeInterval(3.5))

        // Verify reconnection attempted
        XCTAssertTrue(sut.isConnected)
    }

    func testOfflineMapCaching() {
        // Download map, then kill connection
        sut.downloadMap(id: "garden_1")
        let map = database.fetchCachedMap(id: "garden_1")
        XCTAssertNotNil(map?.geojson)
    }
}
```

---

## 5. Integration & Release Plan

### 5.1 Timeline & Milestones

```
Week 1-4:   P3.1 OTA + P3.2 Sensor Fusion (parallel)
Week 2-5:   P3.4 Mobile Native App scaffolding
Week 5-10:  P3.3 Adaptive Learning + P3.4 core features
Week 10-12: Integration + UAT
Week 12-14: Release P3.0

Risk: P3.2 (Eigen library integration) — mitigate early
Critical Path: OTA + Fusion needed before Learning makes sense
```

### 5.2 Dependencies

```
P3.1 OTA
  ← Bootloader (external project, parallel)
  ← Crypto library (mbedtls via FetchContent)

P3.2 Sensor Fusion
  ← A* Pathfinding (P2, not P3)
  ← Eigen3 library (FetchContent)

P3.3 Adaptive Learning
  ← P3.2 completed (needs fusion metrics)
  ← TensorFlow Lite (FetchContent)
  ← Python training helper script

P3.4 Mobile Native
  ← Frozen WebSocket API (already stable)
  ← No hard blocker (can start anytime)
```

### 5.3 Success Criteria

| P3 Feature | Success Metric | Acceptance |
|-----------|----------------|-----------|
| **OTA** | Update success rate | ≥99.5% without corruption |
| **Fusion** | Position RMSE | <2cm during GPS glitch |
| **Learning** | Efficiency gain | +25% after 10 sessions |
| **Mobile** | App store rating | ≥4.5 stars (usability) |

---

## References & Dependencies

- **OTA:** mbedtls, dual-slot bootloader (ARM CMSIS)
- **Fusion:** Eigen3 linear algebra, UKF/EKF literature
- **Learning:** TensorFlow Lite C++, ONNX export tools
- **Mobile:** Mapbox GL Native, SQLite, MQTT client (Swift/Kotlin)

---

**Document Version:** 2026-03-24
**Status:** Design Review Phase
**Next Step:** Prototype P3.1 OTA (weeks after P2.0 release)
