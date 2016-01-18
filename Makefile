# Build settings:
WORK_DIR ?= build
INSTALL_PATH ?= /usr/local

# Set latest Git revision as Version
ifeq (,$(wildcard src/Version.h))
$(shell cp "src/Version.h.in" "src/Version.h")
endif

GIT_REV = $(shell git rev-parse --short HEAD)
SRC_REV = $(shell grep -o "\".*\"" src/Version.h)
GIT_REV_COMPARE = $(shell [ $(GIT_REV) !=  $(SRC_REV) ] && echo true)

ifeq ($(GIT_REV_COMPARE),true)
$(shell sed -i '/#define ABC_VERSION ".*"/c\#define ABC_VERSION "$(GIT_REV)"' src/Version.h )
endif

# Compiler options:
CFLAGS   += -D_GNU_SOURCE -DDEBUG -g -Wall -fPIC -std=c99
CXXFLAGS += -D_GNU_SOURCE -DDEBUG -g -Wall -fPIC -std=c++11
deps = jansson libbitcoin libcurl libgit2 libqrencode libsecp256k1 libssl libzmq protobuf-lite zlib
LIBS := $(shell pkg-config --libs --static $(deps)) \
	-lsodium \
	-lcsv -lm

# Do not use -lpthread on Android:
ifneq (,$(findstring android,$(CC)))
	CFLAGS += -DANDROID
	CXXFLAGS += -DANDROID
	LIBS := $(filter-out -lpthread,$(LIBS)) -llog
endif

# Source files:
abc_sources = \
	$(wildcard abcd/*.cpp abcd/*/*.cpp src/*.cpp) \
	$(wildcard minilibs/libbitcoin-client/*.cpp) \
	minilibs/git-sync/sync.c \
	minilibs/scrypt/crypto_scrypt.c \
	codegen/paymentrequest.pb.cpp

cli_sources = $(wildcard cli/*.cpp cli/*/*.cpp)
test_sources = $(wildcard test/*.cpp)
watcher_sources = $(wildcard util/*.cpp)

generated_headers = \
	codegen/paymentrequest.pb.h

# Objects:
abc_objects = $(addprefix $(WORK_DIR)/, $(addsuffix .o, $(basename $(abc_sources))))
cli_objects = $(addprefix $(WORK_DIR)/, $(addsuffix .o, $(basename $(cli_sources))))
test_objects = $(addprefix $(WORK_DIR)/, $(addsuffix .o, $(basename $(test_sources))))
watcher_objects = $(addprefix $(WORK_DIR)/, $(addsuffix .o, $(basename $(watcher_sources))))

# Adjustable verbosity:
V ?= 0
ifeq ($V,0)
	RUN = @echo Making $@...;
endif

# Targets:
all: $(WORK_DIR)/abc-cli check $(WORK_DIR)/abc-watcher format-check
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

$(WORK_DIR)/abc-watcher: $(watcher_objects) $(WORK_DIR)/libabc.a
	$(RUN) $(CXX) -o $@ $^ $(LDFLAGS) $(LIBS)

check: $(WORK_DIR)/abc-test
	$(RUN) $<

format:
	@astyle --options=astyle-options -Q --suffix=none --recursive --exclude=build --exclude=codegen --exclude=deps --exclude=minilibs "*.cpp" "*.hpp" "*.h"

format-check:
ifneq (, $(shell which astyle))
	@astyle --options=astyle-options -Q --suffix=none --recursive --exclude=build --exclude=codegen --exclude=deps --exclude=minilibs "*.cpp" "*.hpp" "*.h" \
	--dry-run | sed -n '/Formatted/s/Formatted/Needs formatting:/p'
endif

doc: cli/doc/abc-cli.html cli/doc/abc-cli.1

cli/doc/abc-cli.html: cli/doc/abc-cli.pod
	$(RUN) pod2html --infile=$< --outfile=$@ --noindex

cli/doc/abc-cli.1: cli/doc/abc-cli.pod
	$(RUN) pod2man $< $@

clean:
	$(RM) -r $(WORK_DIR) codegen

install: $(WORK_DIR)/libabc.a $(WORK_DIR)/abc-cli cli/doc/abc-cli.1
	install -d $(INSTALL_PATH)/lib
	install -d $(INSTALL_PATH)/bin
	install -d $(INSTALL_PATH)/include
	install -d $(INSTALL_PATH)/share/man/man1
	install -d $(INSTALL_PATH)/share/bash-completion/completions
	install $(WORK_DIR)/libabc.a $(INSTALL_PATH)/lib
	install $(WORK_DIR)/abc-cli $(INSTALL_PATH)/bin
	install src/ABC.h $(INSTALL_PATH)/include
	install cli/doc/abc-cli.1 $(INSTALL_PATH)/share/man/man1
	install cli/abc-cli-bash-completion.sh $(INSTALL_PATH)/share/bash-completion/completions

uninstall:
	rm -f $(INSTALL_PATH)/lib/libabc.a
	rm -f $(INSTALL_PATH)/bin/abc-cli
	rm -f $(INSTALL_PATH)/include/ABC.h
	rm -f $(INSTALL_PATH)/share/man/man1/abc-cli.1
	rm -f $(INSTALL_PATH)/share/bash-completion/completions/abc-cli-bash-completion.sh

# Automatic dependency rules:
$(WORK_DIR)/%.o: %.c | $(generated_headers)
	@mkdir -p $(dir $@)
	$(RUN) $(CC) -c -MD $(CFLAGS) -o $@ $<

$(WORK_DIR)/%.o: %.cpp | $(generated_headers)
	@mkdir -p $(dir $@)
	$(RUN) $(CXX) -c -MD $(CXXFLAGS) -o $@ $<

include $(wildcard $(WORK_DIR)/*/*.d $(WORK_DIR)/*/*/*.d)
%.h: ;
%.hpp: ;

# Protobuf files:
codegen/paymentrequest.pb.h codegen/paymentrequest.pb.cc: abcd/spend/paymentrequest.proto
	@mkdir -p $(dir $@)
	$(RUN) protoc --cpp_out=$(dir $@) --proto_path=$(dir $<) $<

codegen/paymentrequest.pb.cpp: codegen/paymentrequest.pb.cc
	@cp $^ $@
