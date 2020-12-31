#!/bin/bash
count=$1
if [ -e /tmp/$PPID.count ]
then
	count=$(cat /tmp/$PPID.count)
fi
if [ $count -eq 0 ]
then
	echo time to succeed!
	exit 0
fi

echo i will succeed after $count tries
count=$((count-1))
echo $count more to go
echo $count > /tmp/$PPID.count
exit $count
