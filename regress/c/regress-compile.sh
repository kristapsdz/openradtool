#!/bin/sh

pkgs="sqlbox kcgi kcgi-json"

if [ -z "$CFLAGS" ]
then
	CFLAGS="-O2 -pipe -g -W -Wall -Wextra `pkg-config --cflags $pkgs`"
fi

if [ -z "${CC}" ]
then
	CC=cc
fi

for f in regress/*.ort
do
	hf=`basename $f`.h
	set -e
	./ort-c-header -vJj $f >$f.h 2>/dev/null
	./ort-c-source -S. -h $hf -vJj $f >$f.c 2>/dev/null
	set +e
	$CC $CFLAGS -o /dev/null -c $f.c 2>/dev/null
	if [ $? -ne 0 ] ; then
		echo "$CC: $f... fail"
		$CC $CFLAGS -o /dev/null -c $f.c
		rm -f $f.h $f.c
		exit 1
	fi
	rm -f $f.h $f.c
	echo "$CC: $f... pass"
done
