#region Usage
# Host tests of the firmware logic (on the PC, no board).
#   make                -> builds and runs
#   make FILTER=evolve  -> only the tests whose name contains "evolve"
#   make asan           -> the same suite under AddressSanitizer + UBSan
#   make clean
# Every object is compiled separately with -MMD -MP, so editing a header
# (pet.h, dex.h, i18n.h, battle.h, audio.h, ...) rebuilds what includes it.
#endregion

#region Configuration
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O1 -g -Wall -Wextra -Wno-unused-parameter \
            -fno-omit-frame-pointer
CPPFLAGS += -I shim
DEPFLAGS := -MMD -MP
LDFLAGS  ?=
ASAN_FLAGS := -fsanitize=address,undefined -fno-sanitize-recover=all

SRC_DIR    := ..
BUILD      := build
NORMAL_DIR := $(BUILD)/normal
ASAN_DIR   := $(BUILD)/asan
TEST_BINARY := $(BUILD)/tamapoke_tests
ASAN_BINARY := $(BUILD)/tamapoke_tests_asan
#endregion

#region Sources and objects
HARNESS_SOURCES  := main.cpp shim/shim.cpp
TEST_SOURCES     := $(wildcard test_*.cpp)
FIRMWARE_SOURCES := $(SRC_DIR)/pet.cpp $(SRC_DIR)/pet_storage.cpp \
                    $(SRC_DIR)/i18n.cpp $(SRC_DIR)/battle.cpp
SOURCES := $(HARNESS_SOURCES) $(TEST_SOURCES) $(FIRMWARE_SOURCES)

# ../pet.cpp -> <dir>/firmware/pet.o, shim/shim.cpp -> <dir>/shim/shim.o
objects_in = $(addprefix $(1)/,$(subst $(SRC_DIR)/,firmware/,$(SOURCES:.cpp=.o)))
NORMAL_OBJECTS := $(call objects_in,$(NORMAL_DIR))
ASAN_OBJECTS   := $(call objects_in,$(ASAN_DIR))
#endregion

#region Targets
.PHONY: all run clean asan
all: run

run: $(TEST_BINARY)
	@./$(TEST_BINARY) $(FILTER)

asan: $(ASAN_BINARY)
	@./$(ASAN_BINARY) $(FILTER)

$(TEST_BINARY): $(NORMAL_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(ASAN_BINARY): $(ASAN_OBJECTS)
	$(CXX) $(CXXFLAGS) $(ASAN_FLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(BUILD)
#endregion

#region Compile rules
$(NORMAL_DIR)/firmware/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPFLAGS) -c -o $@ $<

$(NORMAL_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPFLAGS) -c -o $@ $<

$(ASAN_DIR)/firmware/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ASAN_FLAGS) $(DEPFLAGS) -c -o $@ $<

$(ASAN_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ASAN_FLAGS) $(DEPFLAGS) -c -o $@ $<
#endregion

#region Header dependencies
-include $(NORMAL_OBJECTS:.o=.d) $(ASAN_OBJECTS:.o=.d)
#endregion
