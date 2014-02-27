HMUC - Haifa's Minimal Unsatisfiable Core solver.
==============================================================
Based on Minisat 2.2 (http://minisat.se/downloads/minisat-2.2.0.tar.gz)

Written by Vadim Ryvchin.




Executable:
-----------

The file hmuc is a statically built version of the solver on Linux (64-bit).



Build from source files:
------------------------

1. Make sure that zlib is installed on your machine (http://zlib.net/)

2. Update the directory location in the file build.sh

3. Make sure build.sh is executable (if not then 'chmod +x build.sh' will fix this)

4. run build.sh



Usage:
------
To run the solver simply type:

hmuc <benchmark file> 

The benchmark file if of dimacs (cnf) format.

The output of hmuc includes a line starting with 'v', that lists
the clauses ids that participate in the proof. 




Usage examples:
---------------
1. The default (no parameters) is what was sent to the competition:

hhlmuc test/ex.cnf



Further information:
--------------------

Questions should be referred to: vadimryv@gmail.com
