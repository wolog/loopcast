#!/bin/sh

if [ "$(/sbin/route -n | grep '^224\.0')" = "" ]; then
	echo 
	echo "NO MULTICAST ROUTE !!"
	echo "route add -net 224.0.0.0 netmask 240.0.0.0 dev eth0"
	echo
	exit 255
fi
 
for TEST in $(ls ./tests/* | grep -v '^./tests/00-'); do
	echo "----------- start $TEST ------------"
	$TEST
	echo "----------- stop $TEST -------------"
	echo 
done
