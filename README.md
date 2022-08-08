## Building

Please execute the following commands to build the app:
``` bash
(mkdir build; cd build; cmake -DCMAKE_BUILD_TYPE=Release ..; make -j$(nproc) )
```


## Running
``` bash
Usage: test1 -c NTHREADS -f FILENAME -m
  -c NTHREADS - unsigned - the number of threads to be used.
    If unspecified, the test uses one thread per the available CPU.
  -f FILENAME - the file name to read the data from.
  -m - measure the size threshold.
```
