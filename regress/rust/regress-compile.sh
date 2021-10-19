#! /bin/sh

rm -f rust/src/*.rs

for f in regress/*.ort
do
	./ort-rust $f >rust/src/lib.rs 2>/dev/null
	if [ $? -ne 0 ]
	then
		echo "ort-rust: $f... fail (did not execute)" 1>&2
		echo "ort-rust: $f... trace:" 1>&2
		./ort-rust $f >/dev/null
		exit 1
	fi
	cd rust
	cargo build --offline 1>/dev/null 2>/dev/null
	if [ $? -ne 0 ]
	then
		echo "ort-rust: $f... fail" 1>&2
		echo "ort-rust: $f... trace:" 1>&2
		cargo build --offline >/dev/null
		exit 1
	fi
	cd ..
	echo "ort-rust: $f... ok"
done
