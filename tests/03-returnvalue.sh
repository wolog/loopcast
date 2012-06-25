#!/bin/sh

OPTRECV="-v -r"
OPTSEND="-v -k -r 100"

(
    sleep 1
    ./looprecv $OPTRECV > /dev/null
	RET=$?
	if [ "$RET" != "100" ]; then 
		echo -n "FAIL: " 
	fi
	echo "Exit is '$RET', expected '100'"
) &
cat test.rand.in | ./loopsend $OPTSEND
killall looprecv 2> /dev/null
sleep 1
killall -KILL looprecv 2> /dev/null
