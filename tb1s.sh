rm -rf _tb1-prof
../libtsdb/mkprof -r ../libtsdb/Relations _tb1-prof
../libtsdb/art -a '../ace/ace -g ../ace/erg.dat -O --max-chart-megabytes=6000 --max-unpack-megabytes=7000' -f _tb1-prof
./fftb -g ../ace/erg.dat -b _tb1-prof
