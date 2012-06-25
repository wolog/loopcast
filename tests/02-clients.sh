#!/bin/sh

FILE=/tmp/looprecv.data

./tests/00-skel-simple.sh "-v -N 1" "-v -k -r 2" "wait one client to start - verbose"
./tests/00-skel-simple.sh "-N 2 -o $FILE" "-k -r 1" "wait two clients to start (and send sigusr1 and sigusr2) - silent" &
PID=$!
sleep 5
killall -SIGUSR2 loopsend
sleep 7
killall -SIGUSR2 loopsend
sleep 1
killall -SIGUSR2 loopsend
sleep 1
killall -SIGUSR1 loopsend
killall -SIGUSR1 loopsend
wait $PID
echo output of $FILE
cat $FILE
rm $FILE
