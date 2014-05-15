BUILD_DIR ?= ../deps/build

ifndef TARGET
	TARGET = native
	LDFLAGS += -L$(BUILD_DIR)/prefix/$(TARGET)/lib -labc
else
	LDFLAGS += -labc
endif

BIN_DIR = ./build/$(TARGET)/bin

ifeq (native, $(TARGET))
	LDFLAGS += -lpthread
endif

CFLAGS   += -Wall -D_GNU_SOURCE -I../src -std=c99
CXXFLAGS += -Wall -D_GNU_SOURCE -I../src -std=c++11

$(BIN_DIR)/test_core: test_core.o
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

default: all

test_cases: $(BIN_DIR)/test_core

all: $(test_cases)

clean:
	@rm -rf $(OBJDIR) *.o $(test_cases)

LD_LIBRARY_PATH=$(BUILD_DIR)/prefix/$(TARGET)/lib/
run: $(test_cases)
	$<

run_core: $(BIN_DIR)/test_core
	$<

gdb_core: $(BIN_DIR)/test_core
	gdb $<

.PHONY: all clean run objdir
