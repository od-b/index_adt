## Precursor
If a new data set is imported, it will initially be tested with assertive methods and all debugging flags.
The tests themselves will then be ran using the compiler flags `-O2`, adding `-g` if profiling is involved.
All tests will be ran on using the following system:
  System Version: macOS 13.2.1 (22D68)
  Kernel Version: Darwin 22.3.0
  Chip: Apple M1, RELEASE_ARM64_T8103 arm64
  Total Number of Cores: 8 (4 performance and 4 efficiency)
  Memory: 8 GB



## 1. Initial Performance
The following tests will be done immediately after the first functioning implementation.
No profiling has been done prior to this - other than simple timestamps using the cacm data set.

### Initial Plan:
From the simple time print added to the search page, the observation was made that time spent in queries increased by factor of 10 or so after adding the scoring algorithm. These observations were made using the CACM set, which is miniscule, and as such - prior to making any changes to the code, a bigger data set is needed.
The initial tests will be run using the data/cacm/ set, creating a baseline for further testing.
Following this, one or more bigger data sets will be indexed, and the results compared.

This process should reveal whether the scoring algorithm continues to scale poorly with larger data sets, and potentially uncover other bottlenecks.



