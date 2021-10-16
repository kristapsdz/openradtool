#! /bin/sh

if [ -n "$1" ]
then
	tmp=$1
else
	ntmp=`mktemp`
	rm -f $ntmp
	tmp=$ntmp.db
	trap "rm -f $tmp" 0
fi

for f in regress/rust/*.ort
do
	rm -f $tmp
	./ort-sql $f | sqlite3 $tmp
	if [ $? -ne 0 ]
	then
		echo "ort-rust $f... fail (ort-sql)" 1>&2
		exit 1
	fi
	cp regress/rust/`basename $f .ort`.rs rust/src/main.rs
	./ort-rust $f >rust/src/lib.rs 2>/dev/null
	if [ $? -ne 0 ]
	then
		echo "ort-rust $f... fail (ort-rust)" 1>&2
		echo "ort-rust $f... trace..." 1>&2
		./ort-rust $f
		exit 1
	fi
	cd rust
	cargo build --offline 1>/dev/null 2>/dev/null
	if [ $? -ne 0 ]
	then
		echo "ort-rust $f... fail (cargo)" 1>&2
		echo "ort-rust $f... trace..." 1>&2
		cargo build --offline
		exit 1
	fi
	cd ..
	./rust/target/debug/orb $tmp
	if [ $? -ne 0 ]
	then
		echo "ort-rust $f... fail (exec)" 1>&2
		exit 1
	fi
	echo "ort-rust $f... ok"
done
