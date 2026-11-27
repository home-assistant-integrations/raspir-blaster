# Training IR remotes in Home Assistant

How a user teaches Home Assistant the buttons of an IR remote with
`raspir-blaster`, and how the learned codes are stored and sent. Everything
runs in Home Assistant: two scripts and MQTT Discovery. There are no firmware
changes and no custom integration.

Status: implemented in Home Assistant 2026.11.3 on 2026-11-26 as
`script.ir_learn_code` and `script.ir_forget_code` (see [Test plan](#9-test-plan)
for what has been verified). It depends on the "IR Event" entity
(`openspec/specs/ha-discovery`, "Discovery of the receive event", PR #16).

## 1. Goal

A user can:

1. **Learn** a button: give it an identifier (remote + button), press the
   button on the remote, and get a Home Assistant entity for it.
2. **Send** it: press that entity in the UI, on a dashboard or from an
   automation. `raspir-blaster` transmits the learned code.
3. **Re-learn** or **forget** a button.

The learned codes survive restarts of Home Assistant, Mosquitto and
`raspir-blaster`, and they are included in a full Home Assistant backup.

### Non-goals

- **Recognizing remote presses.** Turning an incoming frame back into
  "Pro-Ject Vol−" (e.g. to trigger an automation) is a separate feature. See
  [Future work](#10-future-work).
- **Protocol decoding** or cleaning up timings. The learned code is the
  captured frame, unchanged.
- **Measuring the carrier.** The receiver cannot measure it, so the user picks
  it.
- **Several blasters** in one script. The script targets one `raspir-blaster`.
  A second blaster needs a copy of the script with other variables.
- **Firmware changes.** None are needed.

## 2. Workflow

The user names the button **first** and presses it **second**:

```
User                    Home Assistant                     raspir-blaster
 │ run "Learn IR code"   │                                  │
 │ remote=pro_ject       │                                  │
 │ button=vol_down       │                                  │
 │ carrier=36 kHz ──────▶│ notification: "Press vol_down"   │
 │                       │ wait for the next IR Event (20 s)│
 │ press Vol− on remote ─┼──────────────────────────────────▶ capture frame
 │                       │◀── <topic_base>/received ────────┤
 │                       │ IR Event fires, raw = [...]      │
 │                       │ frame ≥ 10 timings? ──▶ yes      │
 │                       │ publish retained discovery config│
 │                       │ → button.ir_pro_ject_vol_down    │
 │◀── notification:      │                                  │
 │    "Learned, 21 timings"                                 │
```

Why this order and not "press first, then HA asks for a name": Home Assistant
cannot open an input dialog on its own when an event arrives. A script with
`fields:` gets a form from the UI when it is started, and can then wait for the
press. Asking afterwards would need an actionable notification with text input
in the Companion app. That works only on a phone, and every press of any
remote, repeats included, would raise a question.

Sending needs nothing new:

```yaml
actions:
  - action: button.press
    target:
      entity_id: button.ir_pro_ject_vol_down
```

## 3. Data model

| Concept | Home Assistant object | Example |
| --- | --- | --- |
| Remote | Device, linked to the blaster with `via_device` | "Pro-Ject remote", identifier `ir_remote_pro_ject` |
| Button | MQTT `button` entity on that device | `button.ir_pro_ject_vol_down` |
| Learned code | `payload_press` of the button | `{"freq":36000,"raw":[889,…]}` |
| Storage | Retained discovery message on the broker | `homeassistant/button/ir_pro_ject_vol_down/config` |

The user's identifier `remotes.pro_ject.button.vol_down` maps to remote
`pro_ject` and button `vol_down`. Home Assistant entity IDs allow only one dot
(between domain and object ID), so the entity becomes
`button.ir_pro_ject_vol_down`. The prefix `ir_` keeps learned buttons apart
from other buttons. On the device page, Home Assistant shows "Pro-Ject remote,
connected via raspir-blaster (raspir-blaster-wz)".

### Identifier rules

`remote` and `button` must match `^[a-z0-9_]{1,32}$`:

- They become part of an MQTT topic, a unique ID and an entity ID. All three
  allow `[a-z0-9_]`, the MQTT topic level also allows `-`, so `[a-z0-9_]` is
  their common subset.
- 32 characters each keep `ir_<remote>_<button>` at 68 characters, well under
  Home Assistant's 255-character entity ID limit.
- The script rejects anything else. It does not slugify silently: a user who
  types `Vol Down` would otherwise get `vol_down` without noticing, and forget
  the button later under the wrong name.

Display names (`remote_name`, `button_name`) are free text and optional. They
default to the identifiers.

## 4. MQTT messages

### Discovery config (one per button)

Topic: `homeassistant/button/ir_<remote>_<button>/config`, retained, QoS 1.
The object ID equals the unique ID and there is no node ID, as the MQTT
integration recommends.

```json
{
  "name": "Vol−",
  "unique_id": "ir_pro_ject_vol_down",
  "default_entity_id": "button.ir_pro_ject_vol_down",
  "command_topic": "home/ir_blaster/send",
  "payload_press": "{\"freq\":36000,\"raw\":[889,889,889,889,889,889,1778,…]}",
  "qos": 1,
  "retain": false,
  "icon": "mdi:remote",
  "availability_topic": "home/ir_blaster/status",
  "device": {
    "identifiers": ["ir_remote_pro_ject"],
    "name": "Pro-Ject remote",
    "model": "Learned IR remote",
    "manufacturer": "raspir-blaster",
    "via_device": "raspir-blaster-wz"
  },
  "origin": {"name": "raspir-blaster Learn IR code script"}
}
```

Field choices:

- **`payload_press` is a string** holding the JSON frame, the format of
  `<topic_base>/send` (`openspec/specs/ir-message-format`). The script builds
  it with `tojson`, so the inner quotes are escaped correctly.
- **`retain: false`** on the command. The firmware ignores retained commands
  anyway, and this keeps the last command off the broker.
- **`availability_topic`** is the blaster's status topic. A learned button
  shows as unavailable while the blaster is offline, which tells the user why
  pressing it does nothing.
- **`default_entity_id`** makes the entity ID predictable. It is used only
  when the entity is first created, so a user who renames the entity keeps
  their name after a re-learn.
- **`via_device`** is the blaster's `device_id`. Its device must exist, which
  it does once the blaster has connected once.

Size: Home Assistant's `tojson` writes `", "` and `": "` with a space, which
the firmware's JSON tokenizer skips. The largest frame (512 timings of up to
5 digits) gives a `payload_press` of at most about 3.6 KB, below the
firmware's 4096 B limit for `send` (`IR_MAX_PAYLOAD`), and a discovery message
of about 4.2 KB, which Mosquitto and Home Assistant accept.

### Why one message per button, not MQTT device discovery

The MQTT integration also supports device discovery: one message
(`homeassistant/device/<id>/config`) with all of a device's components. That
would be one message per remote. But learning a single button would then mean
read-modify-write of the whole remote's config. A script cannot read a
retained message on demand, so it would need a second copy of the remote's
buttons somewhere. One message per button needs no copy: learning,
re-learning and forgetting each touch one topic.

### Forgetting

An empty retained payload on the button's topic. Home Assistant deletes the
entity, and the broker drops the stored message. When the last button of a
remote is gone, Home Assistant also deletes the remote's device, because
nothing references it any more.

## 5. Scripts

The scripts live in the repository as
[`ha/scripts/ir_learn_code.yaml`](../../ha/scripts/ir_learn_code.yaml) and
[`ha/scripts/ir_forget_code.yaml`](../../ha/scripts/ir_forget_code.yaml), with
installation steps in their header comments: paste into the script editor's
YAML mode, replace the marked values, save as `script.ir_learn_code` /
`script.ir_forget_code`. These files are the reference; a script edited in the
UI afterwards may differ.

Values specific to one installation are script variables, so a script can be
adapted to another blaster in one place:

| Variable | Example | Source |
| --- | --- | --- |
| `blaster_device_id` | `raspir-blaster-wz` | firmware config `device_id` |
| `topic_base` | `home/ir_blaster` | firmware config `topic_base` |
| `ir_event` | `event.wohnzimmer_raspir_blaster_raspir_blaster_wz_ir_event` | the blaster's IR Event entity |
| `min_timings` | `10` | shorter frames are treated as fragments (see 6) |
| `timeout_s` | `20` | time to press the button |

The IR Event entity also appears literally in two places (the offline check
and the `wait_for_trigger`): trigger targets and state conditions do not take
templates. `event.received` requires `options.event_type`; the IR Event has
the single type `received`.

### 5.1 `script.ir_learn_code`: Learn IR code

Steps:

1. Reject invalid IDs. If the user typed a full entity ID (contains a dot),
   the message explains what to enter instead.
2. Stop if the IR Event entity is unavailable (blaster offline).
3. Notify "Press and hold … for about a second" and wait up to `timeout_s`
   for `event.received`; skip frames shorter than `min_timings` and keep
   waiting.
4. On timeout: notify, stop, nothing published.
5. Build the discovery config (`config` variable, without `payload_press`) and
   publish it retained.
6. Wait up to 10 s until `button.<object_id>` exists. Only then notify
   "Learned"; otherwise notify that Home Assistant did not create the button.

Implementation notes:

- **`mode: single`**: one learning run at a time. A second start while one is
  waiting is ignored silently, so two runs cannot both take the same press.
- **`wait_for_trigger` only sees new events**, the ones after the wait has
  started. A frame received before the script ran cannot be learned by
  mistake.
- **`raw` is read from the entity, not from the trigger data.** After the wait,
  `state_attr(ir_event, 'raw')` holds the latest frame. If a repeat frame of
  the same press has arrived in between, it is the same button, so this does
  not matter.
- **`payload_press` is built in the publish template, not in `variables`.**
  Home Assistant turns template output that looks like JSON into an object.
  In the first version `payload_press` was built in the `config` variable; it
  became an object, MQTT discovery rejected the config ("value should be a
  string at 'payload_press'") and the script still reported success. Now the
  publish template is
  `dict(config, payload_press=({'freq': …, 'raw': raw} | tojson)) | tojson`,
  so the inner JSON stays a string, and step 6 catches any rejected config.
- **`notification_id: ir_learn`** replaces the previous notification, so the
  user sees one notification per run that ends in the result.

### 5.2 `script.ir_forget_code`: Forget IR code

Checks the IDs with the same rule (so no topic outside `ir_*` can be cleared),
publishes an empty retained payload to
`homeassistant/button/ir_<remote>_<button>/config` and notifies.

Deleting the entity in the UI (device page → entity → Delete) does the same:
the MQTT integration then clears the retained discovery message itself.

## 6. Capture quality

What the captures from the Pro-Ject remote (RC5) showed, and how the design
handles it:

| Observation | Handling |
| --- | --- |
| A press sends one frame, and repeats every ~114 ms while the button is held (RC5) | The first complete frame is used; the others are ignored because the wait has ended |
| A quick tap on the Pro-Ject remote often sends only a fragment (7 timings): the remote stops mid-frame when the button is released | The prompt asks the user to **press and hold for about a second**, so complete repeat frames follow the fragment |
| Fragments occur (7 of ~20 timings when the remote is released early or pointed away) | Frames under `min_timings` (10) are skipped, and the script waits for the next one |
| RC5/RC6 send two variants per button (toggle bit flips on every new press), 19 or 21 timings | Either variant is learned. Comparing two presses would reject valid RC5 codes, so the script does not ask for a second press |
| Timings jitter by ±3 % | Kept as captured. Devices accept the jitter, and the timings are not rounded because the protocol is unknown |

`min_timings` = 10 is a heuristic: NEC frames have 67 timings, RC5 19–21,
Sony 25 or more. A real code shorter than 10 timings (e.g. an NEC repeat code
with 3) can still not be learned. That is intended, because such a frame
means nothing on its own.

If a learned button does not work: learn it again closer to the receiver, or
try the other carrier (36/38 kHz).

### Toggle bit (RC5/RC6)

A learned RC5 button always sends the same toggle bit. The receiving device
treats two presses in a row with the same toggle bit as "held". For
volume, held means "keep changing", which works. For a toggle command (power,
mute), the second press may be ignored. A user can learn the button twice,
once per variant (e.g. `power` and `power_b`), and alternate between them.
Doing that automatically is [future work](#10-future-work).

## 7. Persistence and backup

- **Storage:** retained discovery messages in Mosquitto, written to its
  persistence file in the Mosquitto app's data. They survive restarts of Home
  Assistant, Mosquitto and the blaster.
- **Backup:** a full Home Assistant backup contains the Mosquitto app's data
  and thus the codes. A partial backup without the Mosquitto app does not.
- **Listing / export:** Settings → Devices & services → MQTT → Configure →
  "Listen to a topic" with `homeassistant/button/+/config` shows every
  retained button config with its code; the learned ones are the topics
  starting with `ir_` (MQTT wildcards cannot match a prefix, so buttons of
  other integrations show up too). Copying these messages is a manual
  export.
- **Blaster replaced or `device_id` changed:** the buttons keep working
  through `command_topic` while `topic_base` stays the same. `via_device`
  still points to the old blaster device. Re-learn, or republish with the new
  `blaster_device_id`.

## 8. Usage

- **Learn:** Settings → Automations & scenes → Scripts → "Learn IR code" →
  Run. The form asks for remote, names and carrier. Then press the button
  within 20 s. A dashboard button card with `tap_action: more-info` on
  `script.ir_learn_code` opens the same form.
- **Test:** device page "Pro-Ject remote" → button "Vol−" → Press.
- **Use:** `button.press` in automations and scripts (section 2), or a button
  card on a dashboard.
- **Forget:** script "Forget IR code", or delete the entity in the UI.

## 9. Test plan

On the Zero W with the Pro-Ject remote, Home Assistant 2026.11:

1. Learn `pro_ject` / `vol_down`, 36 kHz: notification "Learned … 21 timings"
   (or 19), entity `button.ir_pro_ject_vol_down` on device "Pro-Ject remote",
   connected via the blaster. **Passed 2026-11-27** as
   `pro_ject_prebox_ds2` / `vol_down` (21 timings, device "Pro-Ject PreBox
   DS2" via raspir-blaster-wz), after two fixes found on the way: quick taps
   gave fragments (prompt now says "hold for about a second"), and
   `payload_press` was rejected as an object (section 5.1).
2. Press the entity: the amp's volume goes down.
3. Learn `vol_up` too: a second entity on the same device.
4. Start learning and press nothing: after 20 s "No IR code received",
   nothing changed. **Passed 2026-11-26** (run ended at the timeout step after
   19 s, no entity created).
5. Start learning with button `Vol Down`: rejected, nothing published.
   **Passed 2026-11-26** (aborted at the ID check).
6. Start learning while the blaster is off: "raspir-blaster is offline".
7. Re-learn `vol_down`: same entity ID, new `payload_press` (check with
   "Listen to a topic").
8. Restart Home Assistant, then Mosquitto: both buttons still there and
   working.
9. Forget `vol_up`: the entity is gone. Forget `vol_down`: the device
   "Pro-Ject remote" is gone too.
10. Unplug the blaster: the learned buttons become unavailable.

## 10. Future work

- **Recognizing learned codes on receive:** match each IR Event against the
  learned buttons and fire one event per button, e.g. an event entity per
  remote with `event_types` = its buttons. Matching needs a tolerance (±25 %
  per timing, same count, or either toggle variant). It could live in the
  learn script's data (the codes are known) or in a macro file
  (`custom_templates/ir.jinja`).
- **Toggle-bit alternation** for RC5/RC6: learn both variants under one
  button and let a script alternate.
- **A Lovelace card** or blueprint for learning, instead of the script form.
- **Learning in the firmware:** storing named codes on the device and sending
  them by name. Rejected for now: every change would need a firmware
  update, and Home Assistant would no longer see the codes.
