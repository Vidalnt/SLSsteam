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

#Default architecture to build for
ARCH := $(shell uname -m)

ifeq ($(shell echo $$CROSS_HOST),X86_64)
	CXX := x86_64-linux-gnu-g++
	CC  := x86_64-linux-gnu-gcc
	ARCH := x86_64
endif
ifeq ($(shell echo $$CROSS_HOST),AArch64)
	CXX := aarch64-linux-gnu-g++
	CC  := aarch64-linux-gnu-gcc
	ARCH := aarch64
endif

libs := $(wildcard lib/$(ARCH)/*.a)
srcs := $(shell find src/ -type f -iname "*.cpp")

CXXFLAGS := -O3 -flto=auto -fPIC -std=c++20 -Wall -Wextra -Wpedantic -Wno-error=format-security -D_GLIBCXX_USE_CXX11_ABI=0
CXXFLAGS += -I$(LUA_INCLUDE)
CXXFLAGS += -floop-block -fgraphite-identity -floop-parallelize-all -pipe -fomit-frame-pointer
CXXFLAGS += $(shell pkg-config --cflags "dbus-1")

LDFLAGS := -shared -Wl,--no-undefined -Wl,--as-needed
LDFLAGS += $(shell pkg-config --libs "libcurl")
LDFLAGS += $(shell pkg-config --libs "dbus-1")

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

ifeq ($(ARCH),x86_64)
	CXXFLAGS += -D "X86_64"
endif
ifeq ($(ARCH),aarch64)
	CXXFLAGS += -D "AARCH64"
endif

#Speed up compilation if additional dependencies are found
ifneq (,$(shell command -v ccache))
	CXX := ccache $(CXX)
endif
#ifneq (,$(shell command -v mold))
#	LDFLAGS += -fuse-ld=mold
#endif

FLAGSSHA := $(shell echo "$(CXXFLAGS) & $(LDFLAGS)" | sha256sum | cut -d " " -f 1)
SLSSTEAMSO := bin/SLSsteam-$(FLAGSSHA).so
objs := $(srcs:src/%.cpp=obj/$(FLAGSSHA)/%.o)
deps := $(objs:%.o=%.d)

audit-libs: embed-version #embed-version belongs to the SLSsteam.so target, yet we can't elegantly add it there. So it stays here for now
	$(MAKE) -j $(JOBS) $(SLSSTEAMSO)

tools:
	$(MAKE) -j 2 schema-grabber ticket-grabber

$(LUA_STAMP):
	@mkdir -p $(LUA_DIR)
	@ok=0; for url in \
		"https://www.lua.org/ftp/lua-$(LUA_VER).tar.gz" \
		"https://snapshot.debian.org/archive/debian/20260831T145707Z/pool/main/l/lua5.4/lua5.4_$(LUA_VER).orig.tar.gz"; do \
		echo "[*] Fetching $$url"; \
		if curl -fL --connect-timeout 10 --retry 2 --retry-delay 1 "$$url" -o "$(LUA_DIR)/lua.tar.gz.tmp"; then \
			mv "$(LUA_DIR)/lua.tar.gz.tmp" "$(LUA_DIR)/lua.tar.gz"; ok=1; break; \
		fi; \
		rm -f "$(LUA_DIR)/lua.tar.gz.tmp"; \
	done; test $$ok -eq 1
	printf '%s  %s\n' "$(LUA_SHA256)" "$(LUA_DIR)/lua.tar.gz" | sha256sum -c -
	tar xzf "$(LUA_DIR)/lua.tar.gz" -C "$(LUA_DIR)" --strip-components=2 "lua-$(LUA_VER)/src"
	rm -f "$(LUA_DIR)/lua.tar.gz" "$(LUA_DIR)/lua.c" "$(LUA_DIR)/luac.c"
	touch "$@"

obj/luavendor/%.o: | $(LUA_STAMP)
	@mkdir -p $(dir $@)
	$(CC) -fPIC -O2 -DLUA_USE_LINUX -I$(LUA_DIR) -c "$(LUA_DIR)/$*.c" -o $@

$(DEFAULT_LUA_A): $(lua_objs)
	@mkdir -p $(dir $@)
	ar rcs $@ $^

$(objs): | $(LUA_STAMP)

$(SLSSTEAMSO): $(objs) $(LUA_A) $(libs)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	ln -f "$@" "bin/SLSsteam.so"

schema-grabber:
	$(MAKE) -C tools/schema-grabber

ticket-grabber:
	$(MAKE) -C tools/ticket-grabber

embed-version:
	$(shell ./embed-version.sh)

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

install: build
	sh setup.sh install

uninstall:
	sh setup.sh uninstall

makepkg-install: build
	-mkdir -p "$$PKGDIR/usr/share/doc/"
	cp -rvL "docs" "$$PKGDIR/usr/share/doc/SLSsteam"

	install -D "bin/SLSsteam.so" "$$PKGDIR/usr/lib/libSLSsteam.so"
	install -D "res/SLSsteam.desktop" "$$PKGDIR/usr/share/applications/SLSsteam.desktop"
	install -D "res/SLSsteam-native.desktop" "$$PKGDIR/usr/share/applications/SLSsteam-native.desktop"
	install -D "res/logo.png" "$$PKGDIR/usr/share/pixmaps/SLSsteam.png"

	install -D "tools/ticket-grabber/bin/Release/net9.0/linux-x64/publish/ticket-grabber" "$$PKGDIR/usr/bin/sls-ticket-grabber"
	install -D "tools/schema-grabber/bin/Release/net9.0/linux-x64/publish/schema-grabber" "$$PKGDIR/usr/bin/sls-schema-grabber"

zips: build
	@mkdir -p zips
	7z a -mx9 -m9=lzma2 \
		"zips/SLSsteam $(DATE).7z" \
		"bin/SLSsteam.so" \
		"setup.sh" \
		"docs/LICENSE" \
		"res/config.yaml" \
		"tools/ticket-grabber/bin/Release/net9.0/linux-x64/publish/ticket-grabber" \
		"tools/schema-grabber/bin/Release/net9.0/linux-x64/publish/schema-grabber"

	#Compatibility for Github issues
	7z a -mx9 -m9=lzma \
		"zips/SLSsteam $(DATE).zip" \
		"bin/SLSsteam.so" \
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
	tools \
	schema-grabber \
	ticket-grabber \
	embed-version \
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
