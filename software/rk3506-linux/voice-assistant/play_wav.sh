#!/bin/sh
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
. "$DIR/config.env"

WAV="${1:-$VOICE_DIR/last_reply.wav}"

if [ -x /userdata/audio-test/es8388_speaker_init.sh ]; then
    /userdata/audio-test/es8388_speaker_init.sh
fi

"$DIR/voice_wav_player" "$WAV" "$PLAYBACK_DEV" "$PLAY_GAIN"
