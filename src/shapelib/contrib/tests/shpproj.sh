#!/bin/sh

cd tests 

rm test*
shpcreate test point

shpadd test -83.54949956              34.992401
shpadd test -83.52162155              34.99276748
shpadd test -84.01681518              34.67275985
shpadd test -84.15596023              34.64862437
shpadd test -83.61951463              34.54927047

dbfcreate test -s 30 fd
dbfadd test "1"
dbfadd test "2"
dbfadd test "3"
dbfadd test "4"
dbfadd test "5"

../shpproj test test_1 -i=geographic -o="init=nad83:1002 units=us-ft"
../shpproj test_1 test_2 -o="proj=utm zone=16 units=m"
../shpproj test_2 test_3 -o=geographic

shpdump  test    > test.out
shpdump  test_3  > test_3.out
result=`diff test.out test_3.out`

if [ -z "${result}" ]; then
  echo success...
else
  echo failure...
fi

rm test*


cd ..