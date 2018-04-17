#!/usr/bin/bash
while true; do
  ./server >& /dev/null &
  sleep 60
done
