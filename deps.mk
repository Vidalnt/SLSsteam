# Single source of truth for the fetched third-party dependency versions + hashes.
#
# Consumed by BOTH:
#   - Makefile               via `include deps.mk`
#   - nix-modules/default.nix via parseDeps (builtins.match over this file)
#
# Edit a version/hash HERE only — never duplicate these values elsewhere. The
# Nix sandbox has no network, so default.nix re-fetches the identical tarballs as
# fixed-output derivations; reading them from this file keeps the two build paths
# from drifting (a mismatch used to be a silent build break).
#
# FORMAT CONTRACT: every dependency fact is a single `NAME := VALUE` line with no
# spaces in VALUE. The nix-side parser matches exactly this shape, so keep new
# entries in the same form (plain assignments, comments start with '#').

LUA_VER    := 5.4.9
LUA_SHA256 := 2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6
