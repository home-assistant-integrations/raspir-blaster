#
#  Makefile
#  raspir-blaster
#
#  Created by Thomas Bonk on 2026-11-20.
#  Copyright 2026 Thomas Bonk <thomas@meandmymac.de>
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

# Top-level build entry point.
#
#   make                       download toolchains + Circle (first run) and build
#                              the kernel images of all targets
#   make TARGETS="rpi0w rpi4"  build only these targets
#   make sdcard                assemble build/sdcard/ and build/cm4-emmc/
#   make test                  build and run the host unit tests for src/core
#   make firmware-checksums    refresh scripts/firmware-files.txt after bumping
#                              a pinned firmware commit
#   make clean                 remove build outputs, keep toolchain/
#   make distclean             also remove toolchain/
#
# Targets: rpi0w (Zero W), rpi3 (Zero 2 W, Pi 3), rpi4 (Pi 4, CM4 Lite),
#          rpi4-emmc (CM4 with eMMC).

ROOT        := $(CURDIR)
ALL_TARGETS := rpi0w rpi3 rpi4 rpi4-emmc
TARGETS     ?= $(ALL_TARGETS)

.PHONY: all bootstrap firmware sdcard test firmware-checksums clean distclean

all: firmware

bootstrap:
	@./scripts/bootstrap.sh $(TARGETS)

firmware: bootstrap
	@set -e; for target in $(TARGETS); do \
		echo "[firmware] building $$target"; \
		mkdir -p build/obj/$$target build/images/$$target; \
		$(MAKE) --no-print-directory -C build/obj/$$target -f $(ROOT)/src/fw/Makefile \
			SRCROOT=$(ROOT) CIRCLEHOME=$(ROOT)/toolchain/circle-$$target; \
		rm -f build/images/$$target/*.img; \
		cp build/obj/$$target/kernel*.img build/images/$$target/; \
	done

sdcard: firmware
	@./scripts/sdcard.sh

test:
	@$(MAKE) --no-print-directory -f tests/Makefile

firmware-checksums:
	@./scripts/firmware-checksums.sh

clean:
	rm -rf build
	@# Outputs of the former in-tree firmware build.
	rm -f src/fw/*.o src/fw/*.d src/fw/kernel*.img src/fw/kernel*.elf \
	      src/fw/kernel*.lst src/fw/kernel*.map

distclean: clean
	rm -rf toolchain
