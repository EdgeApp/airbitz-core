BUILD_DIR ?= build

default: abc curl libbitcoin libqrencode openssl zeromq boost jansson \
         libconfig libwallet obelisk scrypt zlib

$(BUILD_DIR)/%/tasks.mk: recipes/%
	@ ./run.sh $* write-tasks
-include $(patsubst recipes/%,$(BUILD_DIR)/%/tasks.mk,$(wildcard recipes/*))

clean:
	rm -rf $(BUILD_DIR)
