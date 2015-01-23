# Build settings:
WORK_DIR ?= build
INSTALL_DIR ?= /usr/local

# Compiler options:
CFLAGS   += -D_GNU_SOURCE -DDEBUG -g -Wall -fPIC -std=c99
CXXFLAGS += -D_GNU_SOURCE -DDEBUG -g -Wall -fPIC -std=c++11
deps = jansson libbitcoin-watcher libgit2 libqrencode libsecp256k1 libssl libwallet zlib
LIBS := $(shell pkg-config --libs --static $(deps)) \
	-lsodium \
	-lcsv -lcurl -lm

# Do not use -lpthread on Android:
ifneq (,$(findstring android,$(CC)))
	LIBS := $(filter-out -lpthread,$(LIBS))
	CFLAGS += -DANDROID
	CXXFLAGS += -DANDROID
	LIBS += -llog
endif

# Source files:
sources = \
	$(wildcard abcd/*.cpp abcd/*/*.cpp src/*.cpp) \
	minilibs/scrypt/crypto_scrypt.c minilibs/scrypt/sha256.c \
	minilibs/git-sync/sync.c

objects = $(addprefix $(WORK_DIR)/, $(addsuffix .o, $(basename $(sources))))

# Targets:
libabc.so: $(WORK_DIR)/libabc.so $(WORK_DIR)/link-test
libabc.a:  $(WORK_DIR)/libabc.a

$(WORK_DIR)/libabc.a: $(objects)
	$(AR) rcs $@ $^

$(WORK_DIR)/libabc.so: $(objects)
	$(CXX) -shared -o $@ $^ $(LDFLAGS) $(LIBS)

$(WORK_DIR)/link-test: $(WORK_DIR)/libabc.so
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
%.hpp: ;
