#
#  Makefile
#  raspir-blaster tests
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

# Host unit tests for src/core. Run from the repository root:
#   make test        (via the top-level Makefile)
#   make -f tests/Makefile

HOSTCXX   ?= clang++
BUILD_DIR := build/tests

COMMON_FLAGS := -std=c++17 -g -O1 -Wall -Wextra -Werror \
                -fno-exceptions -fno-rtti -fsanitize=address,undefined

# src/core must not depend on the C++ standard library (it is linked into the
# bare-metal firmware), so it is compiled without libc++ headers.
CORE_FLAGS := $(COMMON_FLAGS) -nostdinc++ -I src/core
TEST_FLAGS := $(COMMON_FLAGS) -I src/core -I tests

CORE_SRCS := $(wildcard src/core/*.cpp)
TEST_SRCS := $(wildcard tests/*.cpp)
CORE_OBJS := $(patsubst src/core/%.cpp,$(BUILD_DIR)/core/%.o,$(CORE_SRCS))
TEST_OBJS := $(patsubst tests/%.cpp,$(BUILD_DIR)/%.o,$(TEST_SRCS))

RUNNER := $(BUILD_DIR)/run_tests

# The runner is always relinked so that removed test files drop out of it.
.PHONY: run FORCE scripts
run: $(RUNNER) scripts
	@$(RUNNER)

# Tests of the release helper scripts.
scripts:
	@for t in tests/scripts/test-*.sh; do bash "$$t" || exit 1; done

$(RUNNER): $(CORE_OBJS) $(TEST_OBJS) FORCE
	@echo "  LD    $@"
	@$(HOSTCXX) $(COMMON_FLAGS) -o $@ $(CORE_OBJS) $(TEST_OBJS)

$(BUILD_DIR)/core/%.o: src/core/%.cpp
	@mkdir -p $(dir $@)
	@echo "  CXX   $<"
	@$(HOSTCXX) $(CORE_FLAGS) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	@echo "  CXX   $<"
	@$(HOSTCXX) $(TEST_FLAGS) -MMD -MP -c -o $@ $<

-include $(CORE_OBJS:.o=.d) $(TEST_OBJS:.o=.d)
