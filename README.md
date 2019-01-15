# libmypapi

libmypapi is a library to perform automatic hardware counter measure on both CPUs and GPUs.

## Requisites

  PAPI 5.6.0
  
  Download at <http://icl.utk.edu/papi>


## Installation

     $ make
     
## Execution

Rename your binary that you want libmypapi to handle so that it ends with ```.x```:

     $ PAPI_EVENT=PAPI_TOT_INS LD_PRELOAD=$HOME/libmypapi/libmypapi.so ./app.x
          
Everything else is automatic.

PAPI_EVENT is an environment variable. You can check all PAPI events avaiable by call:

     $ papi_avail

or

     $ papi_native_avail
