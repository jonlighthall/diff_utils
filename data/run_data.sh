# !/bin/bash
#
# BBPP
# frequency
uband_diff bbpp.freq.test.txt bbpp.freq.ref.txt
uband_diff bbpp.freq.test2.txt bbpp.freq.ref.txt
uband_diff bbpp.freq.test.txt bbpp.freq.test2.txt
# replica correlation
uband_diff bbpp.rc.test.txt bbpp.rc.ref.txt
# time series
uband_diff bbpp.ts.test.txt bbpp.ts.ref.txt
#
# BBTL
uband_diff bbtl.pe02.test.txt bbtl.pe02.ref.txt
uband_diff bbtl.pe02.test2.txt bbtl.pe02.ref.txt
uband_diff bbtl.pe02.test.txt bbtl.pe02.test2.txt
#
# uBand
# ducting probability
uband_diff calcpduct.std0.test.txt calcpduct.std0.ref.txt
#
# PE
uband_diff nspe01.asc case1r.tl
uband_diff pe.std1.pe01.test.txt pe.std1.pe01.ref.txt
uband_diff pe.std2.pe02.test.txt pe.std2.pe02.ref.txt
uband_diff pe.std2.pe02.test.txt pe.std2.pe02.test2.txt
uband_diff pe.std3.pe01.test.txt pe.std3.pe01.ref.txt
#
# Pi
# Updated: compare language-specific pi outputs
# Compare C++ generated ascending vs descending
uband_diff pi_cpp_asc.txt pi_cpp_desc.txt
# Compare Fortran generated ascending vs descending
uband_diff pi_fortran_asc.txt pi_fortran_desc.txt
# Optional: compare C++ ascending to Fortran ascending to check cross-language parity
uband_diff pi_cpp_asc.txt pi_fortran_asc.txt
