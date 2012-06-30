#!/bin/bash

TSDBHOME=${LOGONROOT}/lingo/lkb/src/tsdb/home

unset DISPLAY
PROFILE="acetest/gen/$3/$1"
GOLD="acetest/gold/$3"

echo "profile = ${PROFILE}"
echo "gold = ${GOLD}"

rm -rf "${TSDBHOME}/${PROFILE}"

{
echo '(load "testing-preamble.lisp")'

echo '(setf mycpu  :acetest'$1')'
echo '(setf mycpucount '$2')'
echo '(setf myskeleton "'$3'")'
echo '(setf myprofile "'${PROFILE}'")'
echo '(setf myoperation :generate)'
echo '(setf mygold "'${GOLD}'")'

echo '(load "testing-postscript.lisp")'
} | logon -I base -locale en_US.UTF-8 -qq
