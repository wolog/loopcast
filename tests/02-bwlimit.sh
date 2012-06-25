#!/bin/sh

for S in 100000000000 10000; do
	./tests/00-skel-simple.sh "-v -k -w $S" "-k" "bandwidth limit set to $S KiB/s"
done
