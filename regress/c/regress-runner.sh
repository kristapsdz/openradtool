#!/bin/sh

pkgs="sqlbox kcgi-regress kcgi-json libcurl"

if [ -z "$CFLAGS" ]
then
	CFLAGS="-O2 -pipe -g -W -Wall -Wextra `pkg-config --cflags $pkgs`"
fi

if [ -z "$LDADD" ]
then
	LDADD=`pkg-config --libs $pkgs`
fi

if [ -z "${CC}" ]
then
	CC=cc
fi

if [ -n "$1" ]
then
	tmp=$1
else
	ntmp=`mktemp`
	rm -f $ntmp
	tmp=$ntmp.db
fi

trap "rm -f $tmp" 0

for f in regress/c/*.ort
do
	rr=regress/c/regress.c
	bf=regress/c/`basename $f .ort`
	cf=regress/c/`basename $f .ort`.c 
	hf=`basename $f`.h
	rm -f $tmp
	set -e
	./ort-c-header -vJj $f >$f.h 2>/dev/null
	./ort-c-source -S. -h $hf -vJj $f >$f.c 2>/dev/null
	./ort-sql $f | sqlite3 $tmp 2>/dev/null
	set +e
	$CC $CFLAGS -o $bf $f.c $cf $rr $LDADD 2>/dev/null
	if [ $? -ne 0 ] ; then
		echo "$CC: $f... fail (did not compile)"
		$CC $CFLAGS -o $bf $f.c $cf $rr $LDADD
		rm -f $f.h $f.c $bf $tmp
		exit 1
	fi
	rm -f $f.h $f.c
	./$bf $tmp 2>/dev/null
	if [ $? -ne 0 ] ; then
		echo "$CC: $f... fail"
		rm -f $bf $tmp
		exit 1
	fi
	rm -f $bf
	echo "$CC: $f... pass"
done
