# MwCAS Based on AOPT Algorithm

**This reference implementation has been merged into [our MwCAS library](https://github.com/dbgroup-nagoya-u/mwcas)**.

---

[![Ubuntu-20.04](https://github.com/dbgroup-nagoya-u/mwcas-aopt/actions/workflows/unit_tests.yaml/badge.svg)](https://github.com/dbgroup-nagoya-u/mwcas-aopt/actions/workflows/unit_tests.yaml)


This repository is an open source implementation of a multi-word compare-and-swap (MwCAS) operation for research use. This implementation is based on [Guerraoui et al.'s AOPT algorithm](https://drops.dagstuhl.de/opus/volltexte/2020/13082/pdf/LIPIcs-DISC-2020-4.pdf)[^1].

## Build

**Note**: this is a header only library. You can use this without pre-build.

### Prerequisites

```bash
sudo apt update && sudo apt install -y build-essential cmake
cd <your_workspace_dir>
git clone --recurse-submodules git@github.com:dbgroup-nagoya-u/mwcas-aopt.git
cd mwcas-aopt
```

### Build Options

#### Tuning Parameters

- `MWCAS_AOPT_MWCAS_CAPACITY`: the maximum number of target words of MwCAS (default: `4`).
    - In order to maximize performance, it is desirable to specify the minimum number needed. Otherwise, the extra space will pollute the CPU cache.
- `MWCAS_AOPT_FINISHED_DESCRIPTOR_THRESHOLD`: the maximum number of finished descriptors to be retained (default: `64`).

#### Parameters for Unit Testing

- `MWCAS_AOPT_BUILD_TESTS`: build unit tests if `ON` (default: `OFF`).
- `MWCAS_AOPT_TEST_THREAD_NUM`: the number of threads to run unit tests (default: `8`).

### Build and Run Unit Tests

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DMWCAS_AOPT_BUILD_TESTS=ON ..
make -j
ctest -C Release
```

## Acknowledgments

This work is based on results obtained from project JPNP16007 commissioned by the New Energy and Industrial Technology Development Organization (NEDO). In addition, this work was supported partly by KAKENHI (16H01722 and 20K19804).

[^1]: R. Guerraoui, A. Kogan, V. J. Marathe, and I. Zablotchi, "Efficient Multi-word Compare and Swap,” In Proc. DISC, pp. 4:1-4:19, 2020.
