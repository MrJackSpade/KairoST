#!/bin/sh
# KairoST's own checks, after the shared ones (shared/tools/check-app.sh).
set -e
python3 tools/local_patches.py --check
python3 tools/notices.py --check
