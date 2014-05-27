#!/bin/bash

JARLISTS=$(find . -name 'lib*.jar')

GDBSAVE=$(find . -name 'gdb*')
for GDBFILE in $GDBSAVE; do
	BAKFILE=$(echo $GDBFILE | sed 's:.*/::')
	mv $GDBFILE $BAKFILE
done

for JAR in $JARLISTS; do
	echo "Working on $JAR"
	unzip $JAR 'lib/**/*.so' 2>&1 >/dev/null
	zip $JAR -d 'lib/**/*.so' 2>&1 >/dev/null
	zip $JAR -d 'lib/**/' 2>&1 >/dev/null
	zip $JAR -d 'lib/' 2>&1 >/dev/null
done

if [ -d lib ]; then
    ABIS=$(ls lib)
    for ABI in $ABIS; do
	   rm -rf $ABI
	   mv lib/$ABI .
    done
    rmdir lib
fi

for GDBFILE in $GDBSAVE; do
	BAKFILE=$(echo $GDBFILE | sed 's:.*/::')
	mv $BAKFILE $GDBFILE
done
	
