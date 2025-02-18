# Tree-Encoded Bitmaps

This repository contains the source code of
[*Tree-Encoded Bitmaps*](http://db.in.tum.de/~lang/papers/tebs.pdf)
and the short paper *In-Place Updates in Tree-Encoded Bitmaps*, which is an extension of the work done by Lang et al.
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
 
In order to run the experiment measuring the performance of hybrid updates, the test bitmaps need to be generated first using GEN_DATA=1 before running the experiment program proper.
```
make hybrid_v_diff
GEN_DATA=1 ./hybrid_v_diff
./hybrid_v_diff
```
**hybrid_v_diff** measures the update time as well as the memory consumption for both approaches.
 
### Other experiments 
For all the other experiments, they can be run without using GEN_DATA=1 as their test bitmaps are generated at runtime.
```
make <experiment name>
./<experiment name>
```
Some of the experiment names are:
- **diff_v_prune**: measures the performance of run-forming updates with in-place pruning against differential updates with varying merge/prune thresholds. The update time, lookup time, and prune and merge times are measured separately.
- **runbreaking_update_levels**: measures the performance of run-breaking updates based on the length of the broken run. The -res.txt file instead contains lines of two numbers representing every single run-breaking update: the first number is the length of the broken run, while the second number is the update time for that one individual update. This experiment also compares the performance between run-breaking updates and differential updates, although the results are only seen in the -log.txt file. **runbreaking_v_diff** is the experiment that does that, and outputs a corresponding -res.txt file.
- **threshold_v_prune**: measures the performance of the in-place pruning algorithm, while also checking its correctness at the end of the experiment. The prune threshold is varied.
 
## Changes
- *src/dtl/bitmap/teb_flat.hpp* contains most of the changes and additions we made.
- The implementation for hybrid updates can be found in *src/dtl/bitmap/diff/diff.hpp*.
- *experiments/prune* contains the experiments that test the algorithms we proposed.

## Cite Us

To cite our paper, add the following BibTeX snippet to your bibliography:

```
@inproceedings{DBLP:conf/ssdbm/SaputraZPM22,
  author       = {Marcellus Prama Saputra and
                  Eleni Tzirita Zacharatou and
                  Serafeim Papadias and
                  Volker Markl},
  editor       = {Elaheh Pourabbas and
                  Yongluan Zhou and
                  Yuchen Li and
                  Bin Yang},
  title        = {In-Place Updates in Tree-Encoded Bitmaps},
  booktitle    = {{SSDBM} 2022: 34th International Conference on Scientific and Statistical
                  Database Management, Copenhagen, Denmark, July 6 - 8, 2022},
  pages        = {18:1--18:4},
  publisher    = {{ACM}},
  year         = {2022},
  url          = {https://doi.org/10.1145/3538712.3538745},
  doi          = {10.1145/3538712.3538745},
  timestamp    = {Sun, 02 Oct 2022 16:16:07 +0200},
  biburl       = {https://dblp.org/rec/conf/ssdbm/SaputraZPM22.bib},
  bibsource    = {dblp computer science bibliography, https://dblp.org}
}
```
