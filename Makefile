# Build settings:
WORK_DIR ?= build

# Compiler options:
CFLAGS   += -D_GNU_SOURCE -DDEBUG -g -Wall -fPIC -std=c99
CXXFLAGS += -D_GNU_SOURCE -DDEBUG -g -Wall -fPIC -std=c++11
deps = jansson libbitcoin-watcher libgit2 libqrencode libsecp256k1 libssl libwallet zlib
LIBS := $(shell pkg-config --libs --static $(deps)) \
	-lsodium \
	-lcsv -lcurl -lm

# Do not use -lpthread on Android:
ifneq (,$(findstring android,$(CC)))
	CFLAGS += -DANDROID
	CXXFLAGS += -DANDROID
	LIBS := $(filter-out -lpthread,$(LIBS)) -llog
endif

# Source files:
abc_sources = \
	$(wildcard abcd/*.cpp abcd/*/*.cpp src/*.cpp) \
	minilibs/scrypt/crypto_scrypt.c minilibs/scrypt/sha256.c \
	minilibs/git-sync/sync.c

cli_sources = $(wildcard cli/*.cpp)
test_sources = $(wildcard test/*.cpp)

generated_headers = abcd/config.h

# Objects:
abc_objects = $(addprefix $(WORK_DIR)/, $(addsuffix .o, $(basename $(abc_sources))))
cli_objects = $(addprefix $(WORK_DIR)/, $(addsuffix .o, $(basename $(cli_sources))))
test_objects = $(addprefix $(WORK_DIR)/, $(addsuffix .o, $(basename $(test_sources))))

# Adjustable verbosity:
V ?= 0
ifeq ($V,0)
	RUN = @echo Making $@...;
endif

# Targets:
all: $(WORK_DIR)/abc-cli check
libabc.a:  $(WORK_DIR)/libabc.a
libabc.so: $(WORK_DIR)/libabc.so

$(WORK_DIR)/libabc.a: $(abc_objects)
	$(RUN) $(RM) $@; $(AR) rcs $@ $^

$(WORK_DIR)/libabc.so: $(abc_objects)
	$(RUN) $(CXX) -shared -o $@ $^ $(LDFLAGS) $(LIBS)

$(WORK_DIR)/abc-cli: $(cli_objects) $(WORK_DIR)/libabc.a
	$(RUN) $(CXX) -o $@ $^ $(LDFLAGS) $(LIBS)

$(WORK_DIR)/abc-test: $(test_objects) $(WORK_DIR)/libabc.a
	$(RUN) $(CXX) -o $@ $^ $(LDFLAGS) $(LIBS)

check: $(WORK_DIR)/abc-test
	$(RUN) $<

clean:
	$(RM) -r build

# Automatic dependency rules:
$(WORK_DIR)/%.o: %.c $(generated_headers)
	@mkdir -p $(dir $@)
	$(RUN) $(CC) -c -MMD $(CFLAGS) -o $@ $<

$(WORK_DIR)/%.o: %.cpp $(generated_headers)
	@mkdir -p $(dir $@)
	$(RUN) $(CXX) -c -MMD $(CXXFLAGS) -o $@ $<

include $(shell find $(WORK_DIR) -name *.d)
%.h: ;
%.hpp: ;

# API key file:
abcd/config.h:
	@echo "error: Please copy abcd/config.h.example to abcd/config.h add you API keys."
	@exit 1
