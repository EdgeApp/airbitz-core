# Build settings:
WORK_DIR ?= build
INSTALL_DIR ?= /usr/local

# Compiler options:
CFLAGS   += -D_GNU_SOURCE -DDEBUG -g -Wall -fPIC -std=c99
CXXFLAGS += -D_GNU_SOURCE -DDEBUG -g -Wall -fPIC -std=c++11
DEPS = jansson libbitcoin-watcher libqrencode libsecp256k1 libssl zlib
LIBS := $(shell pkg-config --libs --static $(DEPS)) \
        -lsodium \
        -lcurl -lscrypt -lm

# Do not use -lpthread on Android:
ifneq (,$(findstring android,$(CC)))
	LIBS := $(filter-out -lpthread,$(LIBS))
endif

# Top-level targets:
libabc.so: $(WORK_DIR)/libabc.so link-test
libabc.a:  $(WORK_DIR)/libabc.a

c_sources=$(wildcard src/*.c)
cpp_sources=$(wildcard src/*.cpp)
objects=$(patsubst src/%.c,$(WORK_DIR)/%.o,$(c_sources)) \
        $(patsubst src/%.cpp,$(WORK_DIR)/%.o,$(cpp_sources))

$(WORK_DIR)/libabc.a: $(objects)
	$(AR) rcs $@ $^

$(WORK_DIR)/libabc.so: $(objects)
	$(CXX) -shared -o $@ $^ $(LDFLAGS) $(LIBS)

link-test: $(WORK_DIR)/libabc.so
	$(CXX) -o $(WORK_DIR)/link-test -L$(WORK_DIR) -labc link-test.cpp

clean:
	$(RM) libabc.a
	$(RM) -r build

# Automatic dependency rules:
$(WORK_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) -c -MMD $(CFLAGS) -o $@ $<

$(WORK_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c -MMD $(CXXFLAGS) -o $@ $<

include $(shell find $(WORK_DIR) -name *.d)
%.h: ;
