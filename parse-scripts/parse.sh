#!/bin/bash

TSDBHOME=${LOGONROOT}/lingo/lkb/src/tsdb/home

unset DISPLAY
PROFILE="$1"

echo "profile = ${PROFILE}"

rm -rf "${TSDBHOME}/${PROFILE}"

{
echo '(load "testing-preamble.lisp")'

echo '(setf mycpu  :acetest1010)'
echo '(setf mycpucount '$2')'
echo '(setf myskeleton "'$3'")'
echo '(setf myprofile "'${PROFILE}'")'
echo '(setf myoperation :parse)'

echo '(load "testing-postscript.lisp")'
} | logon -I base -locale en_US.UTF-8 -qq
