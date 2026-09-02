#Thanks to https://stackoverflow.com/questions/52034997/how-to-make-makefile-recompile-when-a-header-file-is-changed for the -MMD & -MP flags
#Without them headers wouldn't trigger recompilation

#Force g++ cause clang crashes on some hooks
CXX := g++
CC := gcc
include deps.mk
DEFAULT_LUA_A := obj/liblua5.4.a
LUA_DIR     ?= third_party/lua
LUA_STAMP   ?= $(LUA_DIR)/.fetched-$(LUA_VER)
LUA_INCLUDE ?= $(LUA_DIR)
LUA_A       ?= $(DEFAULT_LUA_A)
lua_names  := lapi lauxlib lbaselib lcode lcorolib lctype ldblib ldebug ldo \
              ldump lfunc lgc linit liolib llex lmathlib lmem loadlib lobject \
              lopcodes loslib lparser lstate lstring lstrlib ltable ltablib ltm \
              lundump lutf8lib lvm lzio
lua_objs   := $(lua_names:%=obj/luavendor/%.o)


libs := $(filter-out lib/libluajit.a,$(wildcard lib/*.a))
srcs := $(shell find src/ -type f -iname "*.cpp")

CXXFLAGS := -O3 -flto=auto -fPIC -m32 -std=c++20 -Wall -Wextra -Wpedantic -Wno-error=format-security -D_GLIBCXX_USE_CXX11_ABI=0
CXXFLAGS += -I$(LUA_INCLUDE)
CXXFLAGS += -floop-block -fgraphite-identity -floop-parallelize-all -pipe -fopenmp -fomit-frame-pointer

LDFLAGS := -shared -Wl,--no-undefined
LDFLAGS += $(shell pkg-config --libs "openssl")
LDFLAGS += $(shell pkg-config --libs "libcurl")
LDFLAGS += -ldl

JOBS := $(shell nproc)

#DATE := $(shell date "+%Y%m%d%H%M%S")
DATE := $(shell cat res/version.txt)

ifeq ($(shell echo $$TRACE),1)
	CXXFLAGS += -D "TRACE"
endif

ifeq ($(shell echo $$DEBUG),1)
	CXXFLAGS += -D "DEBUG"
else
	CXXFLAGS += -Wno-unused-parameter -Wno-unused-variable
endif

ifeq ($(shell echo $$NATIVE),1)
	CXXFLAGS += -march=native
endif

#Speed up compilation if additional dependencies are found
ifeq ($(shell type ccache &> /dev/null && echo "found"),found)
	export PATH := /usr/lib/ccache/bin:$(PATH)
endif
ifeq ($(shell type mold &> /dev/null && echo "found"),found)
	LDFLAGS += -fuse-ld=mold
endif

FLAGSSHA := $(shell echo "$(CXXFLAGS) & $(LDFLAGS)" | sha256sum | cut -d " " -f 1)
SLSSTEAMSO := bin/SLSsteam-$(FLAGSSHA).so
objs := $(srcs:src/%.cpp=obj/$(FLAGSSHA)/%.o)
deps := $(objs:%.o=%.d)

audit-libs:
	$(MAKE) -j $(JOBS) $(SLSSTEAMSO) library-inject
	$(MAKE) link-bins

library-inject:
	@mkdir -p bin
	$(MAKE) -C tools/library-inject
	$(MAKE) link-bins

tools:
	$(MAKE) -j 2 schema-grabber ticket-grabber

link-bins:
	-test -f "$(SLSSTEAMSO)" && ln -f "$(SLSSTEAMSO)" "bin/SLSsteam.so"
	-test -f "tools/library-inject/library-inject.so" && ln -f "tools/library-inject/library-inject.so" "bin/library-inject.so"

$(LUA_STAMP):
	@mkdir -p $(LUA_DIR)
	curl -fsSL "https://www.lua.org/ftp/lua-$(LUA_VER).tar.gz" -o "$(LUA_DIR)/lua.tar.gz"
	printf '%s  %s\n' "$(LUA_SHA256)" "$(LUA_DIR)/lua.tar.gz" | sha256sum -c -
	tar xzf "$(LUA_DIR)/lua.tar.gz" -C "$(LUA_DIR)" --strip-components=2 "lua-$(LUA_VER)/src"
	rm -f "$(LUA_DIR)/lua.tar.gz" "$(LUA_DIR)/lua.c" "$(LUA_DIR)/luac.c"
	touch "$@"

obj/luavendor/%.o: $(LUA_DIR)/%.c | $(LUA_STAMP)
	@mkdir -p $(dir $@)
	$(CC) -m32 -fPIC -O2 -DLUA_USE_LINUX -I$(LUA_DIR) -c $< -o $@

$(DEFAULT_LUA_A): $(lua_objs)
	@mkdir -p $(dir $@)
	ar rcs $@ $^

$(objs): | $(LUA_STAMP)

$(SLSSTEAMSO): $(objs) $(LUA_A) $(libs)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $^ -o $(SLSSTEAMSO) $(LDFLAGS)
	$(MAKE) link-bins

schema-grabber:
	$(MAKE) -C tools/schema-grabber

ticket-grabber:
	$(MAKE) -C tools/ticket-grabber

-include $(deps)
obj/$(FLAGSSHA)/update.o: src/update.cpp res/version.txt
	$(shell ./embed-version.sh)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -isysteminclude -MMD -MP -c $< -o $@

-include $(deps)
obj/$(FLAGSSHA)/config.o: src/config.cpp res/config.yaml
	$(shell ./embed-config.sh)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -isysteminclude -MMD -MP -c $< -o $@

-include $(deps)
obj/$(FLAGSSHA)/%.o : src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -isysteminclude -MMD -MP -c $< -o $@

clean-libs:
	rm -rvf \
		"obj/" \
		"bin/" \
		"zips/" \
		"tools/ticket-grabber/bin" \
		"tools/ticket-grabber/obj" \
		"tools/schema-grabber/bin" \
		"tools/schema-grabber/obj"

clean-tools:
	-$(MAKE) -C tools/schema-grabber clean
	-$(MAKE) -C tools/ticket-grabber clean

install:
	sh setup.sh install

uninstall:
	sh setup.sh uninstall

zips: build
	@mkdir -p zips
	7z a -mx9 -m9=lzma2 \
		"zips/SLSsteam $(DATE).7z" \
		"bin/SLSsteam.so" \
		"bin/library-inject.so" \
		"setup.sh" \
		"docs/LICENSE" \
		"res/config.yaml" \
		"tools/ticket-grabber/bin/Release/net9.0/linux-x64/publish/ticket-grabber" \
		"tools/schema-grabber/bin/Release/net9.0/linux-x64/publish/schema-grabber"

	#Compatibility for Github issues
	7z a -mx9 -m9=lzma \
		"zips/SLSsteam $(DATE).zip" \
		"bin/SLSsteam.so" \
		"bin/library-inject.so" \
		"setup.sh" \
		"docs/LICENSE" \
		"res/config.yaml" \
		"tools/ticket-grabber/bin/Release/net9.0/linux-x64/publish/ticket-grabber" \
		"tools/schema-grabber/bin/Release/net9.0/linux-x64/publish/schema-grabber"

zips-config:
	7z a -mx9 -m9=lzma "zips/SLSsteam - SLSConfig $(DATE).zip" "$(HOME)/.config/SLSsteam/config.yaml"
	#Compatibility for Github issues
	7z a -mx9 -m9=lzma2 "zips/SLSsteam - SLSConfig $(DATE).7z" "$(HOME)/.config/SLSsteam/config.yaml"


clean: clean-libs clean-tools
build: audit-libs tools
rebuild: clean build
release: rebuild zips

.PHONY: \
	audit-libs \
	library-inject \
	tools \
	link-bins \
	schema-grabber \
	ticket-grabber \
	clean-libs \
	clean-tools \
	install \
	uninstall \
	zips \
	zips-config \
	clean \
	build \
	rebuild \
	release
