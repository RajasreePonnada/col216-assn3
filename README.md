# col216-assn3
#### Simulate L1 cache in C++ for quad core processors, with cache coherence support.

#### Command to run the code under default parameters:
#### ./L1simulate -t app1 -s 6 -E 2 -b 5 


## Usage: 

### ./L1simulate [options]
#### Options:

| Option | Description |
|:------:|:------------|
| `-t <tracefile_base>` | Base name of the 4 trace files (e.g., `app1`). |
| `-s <s>` | Number of set index bits (S = 2^s). |
| `-E <E>` | Associativity (number of lines per set, E > 0). |
| `-b <b>` | Number of block offset bits (B = 2^b, b ≥ 2 for 4-byte words). |
| `-o <outputfile>` | (Optional) File to log output for plotting, etc. |
| `-h` | Print this help message. |
