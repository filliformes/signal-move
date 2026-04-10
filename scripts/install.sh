#!/bin/bash
set -e
MODULE_ID="signal"
MOVE_HOST="${MOVE_HOST:-move.local}"
DEST="/data/UserData/schwung/modules/sound_generators/$MODULE_ID"

echo "Installing $MODULE_ID to $MOVE_HOST..."
ssh ableton@$MOVE_HOST "mkdir -p $DEST"
scp "dist/$MODULE_ID/dsp.so"       "ableton@$MOVE_HOST:$DEST/"
scp "dist/$MODULE_ID/module.json"  "ableton@$MOVE_HOST:$DEST/"
ssh ableton@$MOVE_HOST "chown -R ableton:users $DEST"
echo "Done. Remove and re-add Signal from the FX slot to reload."
