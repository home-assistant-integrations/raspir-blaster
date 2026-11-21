#
#  Makefile
#  raspir-blaster firmware
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

# Circle application Makefile, run out of tree by the top-level Makefile:
#
#   make -C build/obj/<target> -f <root>/src/fw/Makefile \
#        SRCROOT=<root> CIRCLEHOME=<root>/toolchain/circle-<target>
#
# Objects and the kernel image are written to the current directory; the
# image name (kernel.img, kernel8.img, kernel8-rpi4.img) follows from the
# RASPPI/AARCH settings in the target's Circle tree.

ifndef SRCROOT
$(error SRCROOT is not set - build through the top-level Makefile)
endif
ifndef CIRCLEHOME
$(error CIRCLEHOME is not set - build through the top-level Makefile)
endif

# Firmware and hardware-independent core sources; vpath lets the Circle
# pattern rules find them while objects stay in the current directory.
FW_DIR   = $(SRCROOT)/src/fw
CORE_DIR = $(SRCROOT)/src/core
vpath %.cpp $(FW_DIR) $(CORE_DIR)
# build/generated holds firmware_version.h (written by the top-level Makefile).
EXTRAINCLUDE = -I $(FW_DIR) -I $(CORE_DIR) -I $(SRCROOT)/build/generated

CORE_OBJS = $(notdir $(patsubst %.cpp,%.o,$(wildcard $(CORE_DIR)/*.cpp)))

FW_OBJS = main.o kernel.o apiserver.o dhcpservertask.o firmwareupdater.o irmqttclient.o irreceiver.o irtransmitter.o logtee.o

OBJS = $(FW_OBJS) $(CORE_OBJS)

# "make WATCHDOG_TEST=1" builds a test image whose main loop hangs after 30 s.
ifeq ($(WATCHDOG_TEST),1)
DEFINE += -DRASPIR_WATCHDOG_TEST
endif

# Link order: dependent libraries first, libcircle last.
LIBS = $(CIRCLEHOME)/addon/wlan/hostap/wpa_supplicant/libwpa_supplicant.a \
       $(CIRCLEHOME)/addon/wlan/libwlan.a \
       $(CIRCLEHOME)/addon/fatfs/libfatfs.a \
       $(CIRCLEHOME)/addon/SDCard/libsdcard.a \
       $(CIRCLEHOME)/lib/net/libnet.a \
       $(CIRCLEHOME)/lib/sched/libsched.a \
       $(CIRCLEHOME)/lib/usb/gadget/libusbgadget.a \
       $(CIRCLEHOME)/lib/usb/libusb.a \
       $(CIRCLEHOME)/lib/input/libinput.a \
       $(CIRCLEHOME)/lib/fs/libfs.a \
       $(CIRCLEHOME)/lib/libcircle.a

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
