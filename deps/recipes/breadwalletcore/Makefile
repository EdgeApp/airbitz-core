SECP256K1ROOT = ../../libsecp256k1/$(TARGET)
SECP256K1INCLUDE = $(SECP256K1ROOT)/include
ETH = ethereum
RLP = $(ETH)/rlp
UTIL = $(ETH)/Util
EVENT = $(ETH)/Event
LES = $(ETH)/les

BTCOBJS := $(shell ls BR*.c | sed s/\\.c/\.o/g)
ETHOBJS := $(shell ls $(ETH)/BR*.c | sed s/\\.c/\.o/g | sed s:$(ETH)/::)
RLPOBJS := $(shell ls $(RLP)/BR*.c | sed s/\\.c/\.o/g | sed s:$(RLP)/::)
UTILOBJS := $(shell ls $(UTIL)/BR*.c | sed s/\\.c/\.o/g | sed s:$(UTIL)/::)
EVENTOBJS := $(shell ls $(EVENT)/BR*.c | sed s/\\.c/\.o/g | sed s:$(EVENT)/::)
LESOBJS := $(shell ls $(LES)/BR*.c | sed s/\\.c/\.o/g | sed s:$(LES)/::)

.PHONY: build
build: breadwalletcore.a

breadwalletcore.a : check-build-env btc eth rlp util event les
	libtool -static -o breadwalletcore.a $(BTCOBJS) $(ETHOBJS) $(RLPOBJS) $(UTILOBJS) $(EVENTOBJS) $(LESOBJS)

.PHONY: btc eth rlp util event les
btc: $(BTCOBJS)
eth: $(ETHOBJS)
rlp: $(RLPOBJS)
util: $(UTILOBJS)
event: $(EVENTOBJS)
les: $(LESOBJS)

$(BTCOBJS) :
	$(CC) $(CFLAGS) -I$(SECP256K1INCLUDE) -c $*.c

$(ETHOBJS) :
	$(CC) $(CFLAGS) -I. -I$(UTIL) -c $(ETH)/$*.c

$(RLPOBJS) :
	$(CC) $(CFLAGS) -I. -I$(UTIL) -I$(RLP) -c $(RLP)/$*.c

$(UTILOBJS) :
	$(CC) $(CFLAGS) -I. -I$(UTIL) -I$(RLP) -c $(UTIL)/$*.c

$(EVENTOBJS) :
	$(CC) $(CFLAGS) -I$(EVENT) -c $(EVENT)/$*.c

$(LESOBJS) :
	$(CC) $(CFLAGS) -I. -I$(ETH) -I$(RLP) -I$(SECP256K1INCLUDE) -I$(SECP256K1ROOT) -c $(LES)/$*.c

.PHONY: clean
clean:
	rm -f *.o *.a

INSTALL_INCLUDE = $(INSTALL_DIR)/include/breadwalletcore
INSTALL_LIB = $(INSTALL_DIR)/lib

.PHONY: install
install: build check-install-env
	mkdir -p $(INSTALL_INCLUDE)
	cp *.h $(ETH)/BR*.h $(RLP)/BR*.h $(UTIL)/BR*.h $(EVENT)/BR*.h $(LES)/BR*.h $(INSTALL_INCLUDE)/
	mkdir -p $(INSTALL_LIB)
	cp breadwalletcore.a $(INSTALL_LIB)/

.PHONY: check-build-env
check-build-env:
ifndef CC
	  $(error CC is undefined)
endif
ifndef CFLAGS
	  $(error CFLAGS is undefined)
endif
ifndef TARGET
	  $(error TARGET is undefined)
endif

.PHONY: check-install-env
check-install-env:
ifndef INSTALL_DIR
	$(error INSTALL_DIR is undefined)
endif
