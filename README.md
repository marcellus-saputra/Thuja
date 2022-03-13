# Tree-Encoded Bitmaps

This repository contains the source code of
[*Tree-Encoded Bitmaps*](http://db.in.tum.de/~lang/papers/tebs.pdf)
and my Bachelor's thesis, which is an extension of the work done by Lang et al.
The original repository can be found [here](https://github.com/harald-lang/tree-encoded-bitmaps).

## Quick start

### Check out the code and generate make files
```
git clone https://github.com/marcellus-saputra/Thuja.git thuja
git clone https://github.com/peterboncz/bloomfilter-bsd.git dtl
git clone https://github.com/harald-lang/fastbit.git fastbit
cd thuja
mkdir build-debug
cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
```
### Run the experiments
All experiments are stored as their own separate programs.
The results are contained in .txt files generated during the experiment called <experiment name>_log.txt and <experiment name>.res.txt.
The -log.txt files are intended to be more human readable, while the -res.txt files are intended to be parsed.
 
### Hybrid approach vs. differential updates.
 
In order to run the experiment measuring the performance of hybrid updates, the test bitmaps need to be generated first.
```
make hybrid_v_diff
GEN_DATA=1 ./hybrid_v_diff
./hybrid_v_diff
```
### Other experiments 
For all the other experiments, they can be run without using GEN_DATA=1 as their test bitmaps are generated at runtime.
```
make <experiment name>
./<experiment name>
```
