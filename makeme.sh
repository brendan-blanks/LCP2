#!/bin/sh
#script name is makeme.sh

echo "Compiling"
make clean
make
assemble test1 test1.mc
assemble test2 test2.mc
assemble test3 test3.mc
assemble test4 test4.mc
assemble test5 test5.mc
assemble test6 test6.mc
assemble test7 test7.mc

