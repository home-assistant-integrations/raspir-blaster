# HTTP API

`raspir-blaster` serves an HTTP API on TCP port 80 under `/api/`, once its
network is up (normal mode) or its setup access point is open (setup mode).
There is no web page: every response is a JSON object with
`Content-Type: application/json`.

This document is for anyone writing a client against the API (a setup app, a
script, `curl`). The requirements it describes are authoritative in
`openspec/specs/web-update` and `openspec/changes/add-web-update` (while the
change is unreleased); this page is the developer-facing summary.

## Requests and responses

- `GET` requests take parameters in the query string; `POST` requests take
  `application/x-www-form-urlencoded` fields (at most 2 KB), except
  `POST /api/update`, which takes `multipart/form-data`.
- **Every response has HTTP status 200.** This is a limitation of the
  firmware's HTTP library (Circle's `CHTTPDaemon`), not a design choice: it
  always substitutes its own HTML error page for any other status code, with
  no way to override that. So **check the JSON body's `"result"` field, not
  the HTTP status**, to tell success from failure.
  - Success: `"result"` is `"ok"` or another value documented per endpoint
    below (e.g. `"installed"`, `"saved"`, `"restarting"`).
  - Failure: `"result":"error"`, with a `"status"` field carrying the
    *logical* HTTP status the request would have had (400, 403, 404 or 409)
    and a `"message"` describing the problem.
  - **One exception:** a `POST /api/update` body over 8 MB is rejected by
    Circle's HTTP library before the firmware's own code runs, so that one
    response is a real HTTP 413 with an HTML body, not JSON. Treat any
    non-200 HTTP status as "upload rejected, nothing changed."
- Responses never contain the value of `mqtt_password`, `ota_password` or
  the Wi-Fi password — only whether they are set.

### Error body

```json
{"result": "error", "status": 403, "message": "wrong password"}
```

## Password

In **normal mode**, `POST /api/update`, `POST /api/update/confirm` and
`POST /api/restart` require a `password` field equal to `ota_password` from
`raspir-blaster.conf`. Without `ota_password` configured, these endpoints
answer `"status":403` ("updates disabled"). A wrong password is answered
`"status":403` after a deliberate 1 s delay and is logged without the value
you sent.

In **setup mode**, no password is required for any endpoint.

## `GET /api/status`

No password needed, in either mode.

```sh
curl http://<device>/api/status
```

```json
{
  "result": "ok",
  "device_id": "pi-zero-ir",
  "board": "Raspberry Pi Zero W (Circle bare-metal)",
  "version": "2026.11.2",
  "build": "sdcard",
  "mode": "normal",
  "network": "wifi",
  "ip": "192.168.1.42",
  "mqtt": "connected",
  "uptime_s": 3600,
  "update_enabled": true
}
```

| Field | Values |
| --- | --- |
| `build` | `sdcard`, `cm4-emmc` |
| `mode` | `normal`, `setup` |
| `network` | `ethernet`, `wifi`, `access-point` |
| `mqtt` | `connected`, `disconnected` |
| `update_enabled` | whether `POST /api/update` is allowed right now (always `true` in setup mode; in normal mode, whether `ota_password` is set) |

## `POST /api/update`

Uploads a release zip (`raspir-blaster-<version>-sdcard.zip` or
`-cm4-emmc.zip`, downloaded from a GitHub release) and, if it validates,
installs the kernel image for this device's board.

Fields (`multipart/form-data`):

| Field | Required | Notes |
| --- | --- | --- |
| `file` | yes | the release zip, at most 8 MB |
| `password` | normal mode only | `ota_password` |

```sh
curl --form-string password=secret -F file=@raspir-blaster-2026.12.0-sdcard.zip \
     http://<device>/api/update
```

The firmware handles one upload at a time; a second upload while one is
being processed is refused with `"status":409`.

Before installing anything, the upload is checked and refused with
`"status":400` if:
- it is not a readable zip,
- it has no `raspir-blaster-manifest.txt`, or the manifest is malformed,
- the manifest's `build` does not match this device's build,
- it does not contain the kernel image for this device's board, or
- that image's SHA-256 does not match the manifest.

If the upload's version is **newer** than the running firmware (comparing
`yyyy.mm.rm` numerically, field by field), it is installed immediately:

```json
{"result": "installed"}
```

The device reboots about 2 s later (enough time for this response to be
delivered), replacing the current kernel image (the previous one is kept as
`<image>.bak`).

Otherwise — the uploaded version is the **same**, **older**, or either
version is not of the `yyyy.mm.rm` form (e.g. a `dev` build) — nothing is
installed yet:

```json
{
  "result": "confirm_required",
  "installed_version": "2026.11.2",
  "uploaded_version": "2026.11.2",
  "reason": "same",
  "token": "a1b2c3d4e5f6...b0"
}
```

`"reason"` is `"same"`, `"older"` or `"not_comparable"`. The uploaded image is
kept in memory for 10 minutes; call `POST /api/update/confirm` with the token
to install it anyway, `POST /api/update/cancel` to discard it right away, or
do nothing and let it expire.

## `POST /api/update/confirm`

Installs the image kept pending from `POST /api/update`.

| Field | Required | Notes |
| --- | --- | --- |
| `token` | yes | from the `confirm_required` response |
| `password` | normal mode only | `ota_password` |

```sh
curl --form-string password=secret --form-string token=a1b2c3d4e5f6...b0 \
     http://<device>/api/update/confirm
```

An unknown or expired (> 10 minutes old) token is answered `"status":400`.
On success:

```json
{"result": "installed"}
```

The device reboots about 2 s later, as with a direct install.

## `POST /api/update/cancel`

Discards the image kept pending from `POST /api/update`, without installing
it. Nothing on the card changes; a later `POST /api/update/confirm` with the
same token then fails with "no update pending".

| Field | Required | Notes |
| --- | --- | --- |
| `password` | normal mode only | `ota_password` |

```sh
curl --form-string password=secret http://<device>/api/update/cancel
```

```json
{"result": "cancelled"}
```

If nothing is pending (already confirmed, cancelled, or expired):

```json
{"result": "error", "status": 400, "message": "no update pending"}
```

## `POST /api/restart`

Reboots the device about 2 s later, without installing anything (e.g. after
`POST /api/config` in setup mode).

| Field | Required | Notes |
| --- | --- | --- |
| `password` | normal mode only | `ota_password` |

```sh
curl --form-string password=secret http://<device>/api/restart
```

```json
{"result": "restarting"}
```

## `GET /api/config`

Available in both modes. In setup mode, no password is needed; in normal
mode, a `password` query parameter equal to `ota_password` is required (the
same rule as the update and restart endpoints).

```sh
curl http://192.168.4.1/api/config                              # setup mode
curl "http://<device>/api/config?password=secret"               # normal mode
```

```json
{
  "result": "ok",
  "mqtt_host": "",
  "mqtt_port": 1883,
  "mqtt_user_set": false,
  "mqtt_password_set": false,
  "device_id": "pi-zero-ir",
  "topic_base": "home/ir_blaster",
  "ir_frame_gap_ms": 15,
  "ota_password_set": false,
  "wifi_ssid": "",
  "wifi_country": "",
  "wifi_password_set": false
}
```

Credentials are never returned, only whether they are set
(`..._set` fields); `mqtt_user` and `mqtt_password` are included by value
only when applicable (`mqtt_user` if `mqtt_user_set`).

## `POST /api/config`

Available in both modes. In setup mode, no password is needed; in normal
mode, a `password` field equal to `ota_password` is required.

| Field | Required | Notes |
| --- | --- | --- |
| `password` | normal mode only | `ota_password` |
| `mqtt_host`, `mqtt_port`, `mqtt_user`, `mqtt_password`, `device_id`, `topic_base`, `ir_frame_gap_ms`, `ota_password` | no | same rules as the matching key in `raspir-blaster.conf` |
| `wifi_ssid` | no | Wi-Fi network to join (omit to leave Wi-Fi unconfigured) |
| `wifi_password` | no | omit for an open network |
| `wifi_country` | if `wifi_ssid` is given | two letters |

Writes `raspir-blaster.conf` and, if `wifi_ssid` is given,
`wpa_supplicant.conf`. The new configuration takes effect only after
`POST /api/restart`. Any field not given keeps its default (this always
starts from "no configuration" — it does not merge with the current one).

```sh
curl --form-string mqtt_host=192.168.1.10 \
     --form-string wifi_ssid=Home \
     --form-string wifi_password=secret123 \
     --form-string wifi_country=DE \
     http://192.168.4.1/api/config
```

```json
{"result": "saved"}
```

If any field is invalid, nothing is written and the response lists which
ones:

```json
{"result": "error", "status": 400, "message": "invalid field value", "fields": ["mqtt_port"]}
```

## Setup mode

If `raspir-blaster.conf` is missing or invalid, or the board has no Ethernet
and `wpa_supplicant.conf` is missing, the device opens an **open (no
encryption) Wi-Fi access point** named `raspir-blaster-<xxxx>` (the last four
hex digits of its Wi-Fi MAC address), reachable at `192.168.4.1`. It hands
out DHCP addresses `192.168.4.10`–`192.168.4.50`. There is no internet access
through this network — it exists only to reach the device's API, without a
password (`GET`/`POST /api/config` need none either, unlike in normal mode).

The access point and API close after **30 minutes without any API request**;
power-cycle the device to try again.

### Typical setup flow for an app

1. Connect the phone/laptop to the Wi-Fi network `raspir-blaster-<xxxx>`
   (there is no internet on this network while connected).
2. `GET /api/status` to confirm `"mode":"setup"`.
3. `GET /api/config` to show current values, if any.
4. `POST /api/config` with the broker and Wi-Fi details.
5. `POST /api/restart`. The device reboots into normal mode on its own
   network; the access point is gone.

## mDNS

The device publishes an `_http._tcp` service via mDNS/Bonjour (both modes),
named `raspir-blaster (<device_id>)`, with TXT records `version`, `board`,
`mode` and `api=/api/`. It also answers `<device_id>.local` (`device_id` is
used as-is: it may only contain characters already valid in a host name),
e.g.:

```sh
dns-sd -B _http._tcp local        # browse (macOS)
ping pi-zero-ir.local
```
