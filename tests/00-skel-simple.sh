#!/bin/sh

[ "$1" = "" ] && exit
[ "$2" = "" ] && exit
[ "$3" = "" ] && exit

[ "$1" != "-" ] && OPTSEND=$1
[ "$2" != "-" ] && OPTRECV=$2

echo
echo "$3"
echo

killall looprecv 2> /dev/null
rm -f test.rand.out
(
	sleep 3
	bash -c "time ./looprecv $OPTRECV" 2> test.time > test.rand.out
	md5sum test.rand.* > test.md5
) & 
cat test.rand.in | ./loopsend $OPTSEND
killall looprecv 2> /dev/null
sleep 1
killall -KILL looprecv 2> /dev/null
cat test.md5 test.time
killall -KILL loopsend 2> /dev/null
