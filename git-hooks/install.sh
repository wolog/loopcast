#!/bin/sh
WPATH=$(dirname $0)/../.git/hooks
CMD_PRECOMMIT="./git-hooks/pre-commit.sh"

if [ "$1" != "ok_to_proceed" ]; then
	echo 
	echo "This script will create or alter files in $WPATH !"
	echo "please run '$0 ok_to_proceed' if you know what you are doing"
	echo 
	exit
fi

if [ ! -f $WPATH/pre-commit ]; then
	echo "Create a empty $WPATH/pre-commit"
	echo "#!/bin/sh" > $WPATH/pre-commit
	chmod +x $WPATH/pre-commit
fi

if [ "$(grep "$CMD_PRECOMMIT" $WPATH/pre-commit)" = "" ]; then
	echo "Invoke '$CMD_PRECOMMIT' from $WPATH/pre-commit"
	echo "$CMD_PRECOMMIT" >> $WPATH/pre-commit
else
	echo "$WPATH/pre-commit already ok"
fi
