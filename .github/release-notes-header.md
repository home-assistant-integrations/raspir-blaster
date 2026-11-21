<!--
  release-notes-header.md
  raspir-blaster

  Created by Thomas Bonk on 2026-11-20.
  Copyright 2026 Thomas Bonk <thomas@meandmymac.de>

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  Prepended to the generated notes of every GitHub release by
  .github/workflows/release.yml.
-->
## Downloads

| File | Use it for |
| --- | --- |
| `raspir-blaster-<version>-sdcard.zip` | Raspberry Pi Zero W, Zero 2 W, Pi 3 B/B+/A+, Pi 4 B and Compute Module 4 **Lite**: extract the contents onto a FAT32 SD card |
| `raspir-blaster-<version>-cm4-emmc.zip` | Compute Module 4 **with eMMC**: copy the contents onto the eMMC's boot partition (via `rpiboot`) |
| `SHA256SUMS.txt` | Checksums of both zip files |

Verify the download before using it:

```sh
shasum -a 256 -c SHA256SUMS.txt
```

After copying, rename `wpa_supplicant.conf.example` and `raspir-blaster.conf.example` (drop `.example`) and fill in your Wi-Fi and MQTT settings. See the README sections [Installation](https://github.com/home-assistant-integrations/raspir-blaster#installation) and [Configuration](https://github.com/home-assistant-integrations/raspir-blaster#configuration-raspir-blasterconf).
