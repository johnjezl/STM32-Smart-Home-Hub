#!/bin/bash
#
# deploy.sh - Install a development build of the app and dashboard on a running board
#
# Usage: ./tools/deploy.sh [target-ip]
#
# Build it first, for the board: see "Deploying to Device" in CLAUDE.md.
#

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TARGET="root@${1:-smarthub.local}"
BINARY="${SMARTHUB_BINARY:-$PROJECT_DIR/app/build-arm/smarthub}"

if [ ! -f "$BINARY" ]; then
    echo "Error: $BINARY not found. Build it for the board first." >&2
    exit 1
fi

# Sent over ssh stdin, because scp may fail against Dropbear. Installed under a temp name and then
# renamed, which works while the old one is running.
echo "Installing the app..."
ssh "$TARGET" "cat > /usr/bin/smarthub.new && chmod +x /usr/bin/smarthub.new && mv -f /usr/bin/smarthub.new /usr/bin/smarthub" < "$BINARY"

echo "Installing the dashboard..."
ssh "$TARGET" "mkdir -p /opt/smarthub/www"
for file in "$PROJECT_DIR"/web/*.html "$PROJECT_DIR"/web/*.js "$PROJECT_DIR"/web/*.css; do
    ssh "$TARGET" "cat > /opt/smarthub/www/$(basename "$file")" < "$file"
done

# An older image doesn't have the init script
echo "Restarting..."
ssh "$TARGET" 'if [ -x /etc/init.d/S90smarthub ]; then
    /etc/init.d/S90smarthub restart
else
    killall smarthub
    for i in 1 2 3 4 5 6 7 8 9 10; do pidof smarthub > /dev/null || break; sleep 1; done
    nohup smarthub > /var/log/smarthub.log 2>&1 < /dev/null &
fi'

echo "Done"
