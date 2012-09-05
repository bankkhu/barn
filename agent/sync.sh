#!/bin/bash

if [[ "$1" == "" || "$2" == "" ]]; then
  echo "Usage: sync.sh RSYNC_HOST:RSYNC_PORT RSYNC_LOG_TREE";
  echo "   RSYNC_LOG_TREE could be a ls-compatible glob"
  exit 0;
fi

BARN_RSYNC_ADDR=$1
INOTIFY_EXCLUSIONS="--exclude '\.u' --exclude 'lock' --exclude 'current' --exclude '*~'"
RSYNC_EXCLUSIONS="--exclude=*.u --exclude=config --exclude=current --exclude=lock --exclude=*~"
RSYNC_FLAGS="-c --verbose"  # --verbose is important since we use it to issue rsync incrementally
RSYNC_LOG_TREE="$2"

function close {
  echo "Sending TERM to all children. Say goodbye kids!"
  pkill -TERM -P $$
  exit 0
}

#Monitor a subdirectory
function sleepit {
  uname=`uname`
  RSYNC_SOURCE=$1
  echo "Nothing more to sync. Hibernating till a log file is rotated on $RSYNC_SOURCE"
  if [[ "$uname" == 'Linux' ]]; then
     inotifywait $INOTIFY_EXCLUSIONS --timeout 3600 -q -e close_write $RSYNC_SOURCE/
  elif [[ "$uname" == 'Darwin' ]]; then
     echo "I'm on OSX so I'm going to loop-sleep like a madman. Use Linux for inotify."
     sleep 10
  fi
}

# Take one argument and sync it to the target barn rsync server
function sync {
  RSYNC_SOURCE=$1
  BASE_NAME=$(echo $RSYNC_SOURCE | sed 's/\//_/g')
  HOST_NAME=$(hostname -f)

  echo "Checking for $RSYNC_SOURCE"

  RSYNC_COMMAND_LINE="$RSYNC_FLAGS $RSYNC_EXCLUSIONS $RSYNC_SOURCE/* rsync://$BARN_RSYNC_ADDR/barn_logs/$BASE_NAME@$HOST_NAME/"
  CANDIDATES=$(eval "rsync --dry-run $RSYNC_COMMAND_LINE" | grep -v "created directory" | grep "@" | awk '{print $1}' | sort)
  for c in $CANDIDATES; do
    echo "Candidate on $RSYNC_SOURCE is $c"
    rsync $RSYNC_COMMAND_LINE
  done

  if [[ $CANDIDATES == "" ]]; then
    return 1   #I didn't sync anything
  else
    return 0
  fi
}

# Main program is here
for RSYNC_SOURCE in `find $RSYNC_LOG_TREE -type d`; do
  (
    while true; do
      sync $RSYNC_SOURCE || sleepit $RSYNC_SOURCE
    done
  ) &
done

trap close SIGTERM
trap close SIGINT

wait

