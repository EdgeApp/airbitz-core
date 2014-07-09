ifndef TARGET
	TARGET = native
	LDFLAGS += -L../build/ -labc
else
	LDFLAGS += -labc
endif

BIN_DIR = ./build/$(TARGET)/bin

# Use -lpthread everywhere except Android:
ifeq (,$(findstring android,$(CC)))
	LDFLAGS += -lpthread
endif

CFLAGS   += -Wall -D_GNU_SOURCE -I../src -std=c99
CXXFLAGS += -Wall -D_GNU_SOURCE -I../src -std=c++11

$(BIN_DIR)/test_core: test_core.o
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

$(BIN_DIR)/test_send: test_send.o
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

$(BIN_DIR)/test_sync: test_sync.o
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

default: all

test_cases: $(BIN_DIR)/test_send $(BIN_DIR)/test_core $(BIN_DIR)/test_sync

all: $(test_cases)

clean:
	@rm -rf $(OBJDIR) *.o $(test_cases)

LD_LIBRARY_PATH := ../build/:$(LD_LIBRARY_PATH)
run: $(test_cases)
	$<

run_core: $(BIN_DIR)/test_core
	$<

run_send: $(BIN_DIR)/test_send
	$<

run_sync: $(BIN_DIR)/test_sync
	$<

gdb_core: $(BIN_DIR)/test_core
	gdb $<

gdb_send: $(BIN_DIR)/test_send
	gdb $<

gdb_sync: $(BIN_DIR)/test_sync
	$(RM) -r sync_repo
	gdb $<

val_core: $(BIN_DIR)/test_core
	valgrind --leak-check=full $<

.PHONY: all clean run objdir
