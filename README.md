# NetSentryHub — WiFi IDS Aggregation Hub

**Status:** Complete
**Board:** ESP32 dev board
**Role:** Central HTTPS hub — collects WiFi intrusion-detection events from sensor nodes and serves a live dashboard

---

## What It Does

NetSentryHub is the server node of a small WiFi intrusion-detection setup. It runs a secured HTTPS web server and acts as the collection point for whatever a fleet of sensor nodes sees on the air: nearby access points, probe requests, and deauthentication frames. Sensor nodes POST their findings here as JSON; you browse a live dashboard from any device on the same network.

| Capability | Detail |
|---|---|
| **Event ingestion** | Accepts JSON POSTs of network/probe/deauth records from sensor nodes via `/api/events` |
| **Alert ingestion** | Accepts alert POSTs (e.g. rogue AP, deauth flood) via `/api/alerts` |
| **Web dashboard** | Auth-gated dashboard showing live networks, probes, alerts, connected nodes |
| **Session management** | Username/password login with 2-hour session cookie, admin/operator roles |
| **SSD1306 OLED** | Shows AP IP, connected client count, event totals |
| **Report export** | JSON/CSV export of everything the hub has collected |

This project is a from-scratch reference implementation, built to explore what a lightweight WIDS aggregation layer looks like on constrained hardware — auth, TLS, in-memory storage, and a browser dashboard all running on a single ESP32.

---

## Hardware

| Item | Notes |
|---|---|
| ESP32 dev board | Any standard 38-pin ESP32 |
| SSD1306 OLED (128×64) | SPI: CLK=18, MOSI=23, RST=16, DC=4, CS=5 |

No buttons required. NetSentryHub is a server — configure it, flash it, leave it running.

---

## Network Mode

NetSentryHub runs as a **WiFi Access Point** (`NetSentryHub` by default). Sensor nodes and your browser connect to this AP. It does **not** join your home router — it is its own isolated network segment.

AP address: `192.168.4.1`
Dashboard: `https://192.168.4.1/` (accept the self-signed cert warning)

---

## Configuration

Copy `include/secrets.example.h` → `include/secrets.h` and fill in:

```cpp
#define AP_PASSWORD      "your-ap-password"
#define ADMIN_USER       "admin"
#define ADMIN_PASSWORD   "your-dashboard-password"
#define DEVICE_API_KEY   "your-api-key"   // must match every sensor node's API key
```

Copy `include/tls_cert.example.h` → `include/tls_cert.h` and generate your own self-signed cert:

```bash
openssl req -x509 -newkey rsa:2048 \
  -keyout key.pem -out cert.pem -days 3650 -nodes -subj "/CN=NetSentryHub"
```

Key settings in `include/config.h`:

```cpp
#define AP_SSID          "NetSentryHub"
#define AP_CHANNEL       6
#define AP_MAX_CLIENTS   8
#define WEB_PORT         443
#define SESSION_TIMEOUT_MS   (2 * 60 * 60 * 1000)   // 2 hours
```

---

## API Endpoints

| Method | Path | Auth | Purpose |
|---|---|---|---|
| GET | `/` | — | Redirects to login or dashboard |
| GET/POST | `/login` | — | Dashboard login |
| GET | `/logout` | session | Clear session |
| GET | `/dashboard` | session | Main dashboard UI |
| POST | `/api/events` | `X-API-Key` | Ingest network/probe/deauth records from sensor nodes |
| POST | `/api/alerts` | `X-API-Key` | Ingest alerts from sensor nodes |
| GET | `/api/status` | session | JSON status summary |

Sensor nodes must send an `X-API-Key: <DEVICE_API_KEY>` header with every POST.

Event records are typed JSON objects distinguished by a `"type"` field (`network`, `probe`, or `deauth`); anything else is ignored.

---

## Data Capacity (in-memory)

```cpp
MAX_NETWORKS    80
MAX_PROBES     200
MAX_DEAUTHS     50
MAX_ALERTS      80
MAX_DEVICES     20
```

Data is held in RAM — it resets on reboot. For persistent storage, a future enhancement would log to SPIFFS.

---

## Flash

```bash
cd NetSentryHub
pio run -t upload
pio device monitor   # 115200 baud
```

---

## TLS Certificate

The TLS certificate is embedded in `include/tls_cert.h`, which you generate yourself (see Configuration above). It is a self-signed certificate for the device's AP IP. Browsers will warn on first visit — add a permanent exception.

---

## Notes for Reviewers

This is a portfolio-scoped extraction of a larger personal home-network monitoring project. Session tokens use hardware RNG and are checked with `strcmp`; the login password comparison uses `String::operator==`, which is not constant-time — acceptable here since the surface is a local AP rather than a networked service, but worth calling out for anyone adapting this for a different threat model.
