(in-package :tsdb)

(let* (
       (wrapper (logon-file "bin" "logon" :string))
       (options '(#-:runtime-standard "--source" 
                  #+:runtime-standard "--binary" "--tty"))
       (path (format nil "franz/~a" mk::%system-binaries%))
       (binary (logon-file path "alisp" :string))
       (base (logon-file path "base.dxl" :string))
       (cheap (logon-file "bin" "cheap" :string))
       (devace "/home/sweaglesw/cdev/ace/ace")
       (ace-released "/home/sweaglesw/cdev/ace-regression/ace-released")
       (ace-candidate "/home/sweaglesw/cdev/ace-regression/ace-candidate")
	   (erg "ERG (trunk)")
       (wait 300)
       (quantum 180))
	(setf acetest-cpus (list

	  ;; PET on a separate ERG trunk copy, for Woodley's tests
     (make-cpu 
;      :host (short-site-name)
      :host "nautilus"
      :spawn cheap
      :options (list "-tsdb" "-yy" "-packing"
                     "-repp" "-cm" "-default-les=all"
                     "-memlimit=1024" "-timeout=60"
                     "/home/sweaglesw/cdev/erg/english.grm")
      :class :acetestpet :grammar erg :name "pet"
      :task '(:parse)  :flags '(:generics nil)
      :wait wait :quantum quantum)

	  ;; PET on a separate ERG trunk copy, for Woodley's tests
	  ;; with packing off
     (make-cpu 
;      :host (short-site-name)
      :host "nautilus"
      :spawn cheap
      :options (list "-tsdb" "-yy" "-packing=0"
                     "-repp" "-cm" "-default-les=all"
                     "-memlimit=1024" "-timeout=60"
                     "/home/sweaglesw/cdev/erg/english.grm")
      :class :acetestpet+nopack :grammar erg :name "pet"
      :task '(:parse)  :flags '(:generics nil)
      :wait wait :quantum quantum)

	;; parsing and generation using ACE, for regression testing from old to new versions of ACE
     (make-cpu 
      :host "nautilus"
      :spawn ace-released
      :options (list "-g" "/home/sweaglesw/cdev/ace-regression/released.dat" "-t")
      :class :acetestace-released-parse :grammar erg :name "ape" :task '(:parse) :wait wait)
     (make-cpu 
      :host "nautilus"
      :spawn ace-released
      :options (list "-g" "/home/sweaglesw/cdev/ace-regression/released.dat" "-e" "-t")
      :class :acetestace-released-gen :grammar erg :name "age" :task '(:generate) :wait wait)
     (make-cpu 
      :host "nautilus"
      :spawn ace-candidate
      :options (list "-g" "/home/sweaglesw/cdev/ace-regression/candidate.dat" "-t")
      :class :acetestace-candidate-parse :grammar erg :name "ape" :task '(:parse) :wait wait)
     (make-cpu 
      :host "nautilus"
      :spawn ace-candidate
      :options (list "-g" "/home/sweaglesw/cdev/ace-regression/candidate.dat" "-e" "-t")
      :class :acetestace-candidate-gen :grammar erg :name "age" :task '(:generate) :wait wait)

     ;;
     ;; ERG parsing and generation, using the proprietary ACE software
     ;;
     (make-cpu 
;      :host (short-site-name)
      :host "nautilus"
      :spawn devace
      :options (list "-g" "/home/sweaglesw/cdev/erg/erg-no-idiom.dat" "-t")
      :class :acetestape+noidiom :grammar erg :name "ape" :task '(:parse) :wait wait)
     (make-cpu 
;      :host (short-site-name)
      :host "nautilus"
      :spawn devace
      :options (list "-g" "/home/sweaglesw/cdev/erg/erg-no-cm.dat" "-t")
      :class :acetestape+nocm :grammar erg :name "ape" :task '(:parse) :wait wait)
     (make-cpu 
;      :host (short-site-name)
      :host "nautilus"
      :spawn devace
      :options (list "-g" "/home/sweaglesw/cdev/erg/erg-trunk.dat" "-e" "-t")
      :class :acetestage :grammar erg :name "ape" :task '(:generate) :wait wait)

     (make-cpu 
;      :host (short-site-name)
      :host "nautilus"
      :spawn devace
      :options (list "-g" "/home/sweaglesw/cdev/ace/erg-1010.dat" "-t" "--itsdb-forest" "-r" "root_strict root_informal root_frag root_inffrag" "--tnt-model" "/home/sweaglesw/logon/coli/tnt/models/wsj")
      :class :acetest1010 :grammar erg :name "ace" :task '(:parse) :wait wait)

     (make-cpu 
;      :host (short-site-name)
      :host "nautilus"
      :spawn devace
      :options (list "-g" "/home/sweaglesw/cdev/ace/erg.dat" "-t" "--itsdb-forest" "-r" "root_strict root_informal root_frag root_inffrag" "--tnt-model" "/home/sweaglesw/logon/coli/tnt/models/wsj" "--max-chart-megabytes=2000" "--max-unpack-megabytes=2500")
      :class :acetestterg :grammar erg :name "ace" :task '(:parse) :wait wait)

	;; ERG generation with LKB, on a separate ERG trunk copy
     (make-cpu 
      :host (short-site-name)
;      :host "nautilus"
      :spawn wrapper
      :options (append 
                options
                (list "-I" base "-qq" "-locale" "en_US.UTF-8"
                      "-L" (logon-file "lingo" "werggen.lisp" :string)))
      :class :acetestlkb :grammar erg :task '(:parse :generate)
      :wait wait :quantum quantum)
	  )))

;(setf *pvm-cpus* (append *pvm-cpus* acetest-cpus))
(setf *pvm-cpus* acetest-cpus)
