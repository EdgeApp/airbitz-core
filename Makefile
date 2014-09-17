# Build settings:
WORK_DIR ?= build
INSTALL_DIR ?= /usr/local

# Compiler options:
CFLAGS   += -D_GNU_SOURCE -DDEBUG -g -Wall -fPIC -Isrc -Iabcd -std=c99
CXXFLAGS += -D_GNU_SOURCE -DDEBUG -g -Wall -fPIC -Isrc -Iabcd -std=c++11
DEPS = jansson libbitcoin-watcher libgit2 libqrencode libsecp256k1 libssl libwallet zlib
LIBS := $(shell pkg-config --libs --static $(DEPS)) \
        -lsodium \
        -lcurl -lscrypt -lm -lcsv

# Do not use -lpthread on Android:
ifneq (,$(findstring android,$(CC)))
	LIBS := $(filter-out -lpthread,$(LIBS))
	CFLAGS += -DANDROID
	CXXFLAGS += -DANDROID
	LIBS += -llog
endif

# Top-level targets:
libabc.so: $(WORK_DIR)/libabc.so link-test
libabc.a:  $(WORK_DIR)/libabc.a

objects=$(addprefix $(WORK_DIR)/,\
	$(patsubst %.c,%.o,$(wildcard abcd/*.c src/*.c)) \
	$(patsubst %.cpp,%.o,$(wildcard abcd/*.cpp src/*.cpp)))

$(WORK_DIR)/libabc.a: $(objects)
	$(AR) rcs $@ $^

$(WORK_DIR)/libabc.so: $(objects)
	$(CXX) -shared -o $@ $^ $(LDFLAGS) $(LIBS)

link-test: $(WORK_DIR)/libabc.so
	$(CXX) -o $(WORK_DIR)/link-test -L$(WORK_DIR) -labc link-test.cpp

clean:
	$(RM) -r build

# Automatic dependency rules:
$(WORK_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c -MMD $(CFLAGS) -o $@ $<

$(WORK_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c -MMD $(CXXFLAGS) -o $@ $<

include $(shell find $(WORK_DIR) -name *.d)
%.h: ;
