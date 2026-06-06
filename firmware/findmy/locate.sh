#!/usr/bin/env bash
# Wrapper that starts the anisette container, runs request_reports.py, then stops it.
# Usage: same as request_reports.py
#   ./locate.sh               # query last 24h
#   ./locate.sh --hours 168   # query last 7 days
#   ./locate.sh --regen       # force re-authentication

set -e
cd "$(dirname "$0")"

if ! docker ps --format '{{.Names}}' | grep -q '^anisette$'; then
    echo "Starting anisette container..."
    docker start anisette >/dev/null
    # Give it a second to be ready
    sleep 1
    if ! curl -sf http://localhost:6969 >/dev/null; then
        echo "ERROR: anisette container didn't respond on port 6969"
        docker stop anisette >/dev/null 2>&1 || true
        exit 1
    fi
    STARTED=1
else
    STARTED=0
fi

uv run request_reports.py "$@"
EXIT_CODE=$?

if [ "$STARTED" = "1" ]; then
    docker stop anisette >/dev/null
fi

exit $EXIT_CODE
