(setf (system:getenv "DISPLAY") nil)
(load "/home/sweaglesw/logon/lingo/lkb/src/general/loadup.lisp")
(pushnew :lkb *features*)
(tsdb::tsdb :initialize)

(setf (sys:gsgc-parameter :expansion-free-percent-new) 5)
(setf (sys:gsgc-parameter :free-percent-new) 2)
(setf (sys:gsgc-parameter :expansion-free-percent-old) 5)
(setf tsdb::*process-suppress-duplicates* nil)
(setf tsdb::*process-raw-print-trace-p* t)
(setf tsdb::*tsdb-maximal-number-of-results* 1000)
(setf tsdb::*tsdb-exhaustive-p* nil)
(setf tsdb::*tsdb-maximal-number-of-analyses* 1000)
;(setf tsdb::*tsdb-exhaustive-p* t)

(load "/home/sweaglesw/logon/lingo/lkb/src/tsdb/lisp/tsdb.lisp")

(load "/home/sweaglesw/logon/dot.tsdbrc")
(load "acetest-cpus.lisp")

(tsdb:tsdb :skeletons "/home/sweaglesw/logon/lingo/lkb/src/tsdb/skeletons/english")

(setf mygold nil)
