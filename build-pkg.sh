#!/bin/bash
set -e
make audit-libs -j"$(nproc)"
bash pkg/slssteam-local/build.sh
