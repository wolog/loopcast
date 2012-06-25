#!/bin/sh
[ ! -x /usr/bin/indent ] && exit

APPLY=no
INDENT="indent -nbad -bap -nbc -bbo -hnl -br -brs -c33 -cd33 -ncdb -ce -ci4 -cli0 -d0 -di1 -nfc1 -i8 -ip0 -l80 -lp -npcs -nprs -npsl -sai -saf -saw -ncs -nsc -sob -nfca -cp33 -ss -ts8 -il1"
TOTAL=0

if [ "$1" = "yes" ]; then
	APPLY=yes
fi
for F in *.c *.h; do 
	$INDENT < $F > $F.indent
	# indent again to prevent toggle indent
	$INDENT < $F.indent > $F.indent2
	mv $F.indent2 $F.indent
	CHANGES=$(diff -u $F $F.indent | grep '^+' | grep -v '^+++' | wc -l)
	if [ $CHANGES -gt 0 ]; then
		TOTAL=$((TOTAL+1))
	fi
	echo "$F: $CHANGES change(s)"
	if [ "$APPLY" = "yes" ]; then
		chmod --reference=$F $F.indent
		cat $F.indent > $F
	fi
	rm $F.indent
done
if [ $TOTAL -gt 0 ]; then
	if [ "$APPLY" = "yes" ]; then
		echo "$TOTAL files changed, check git status"
	else
		echo "$TOTAL files to be changed, run '$0 yes' to do so"
	fi
	exit 1
else
	echo "Indent ok, you may proceed"
fi
