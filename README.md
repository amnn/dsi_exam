# README

## Building

Building the project is facilitated by `make`. `make clean` will delete all
generated artifacts, and any of `make`, `make all` or `make bin/incdb` will
build the binary, which may be found at `bin/incdb` relative to the root of the
project.

Useful variables that may be set during compilation include `CC` which controls
which C++ compiler is used (defaulting to `g++`) and `DEFINES`, which is
intended to inject preprocessor definitions into every compilation unit
(defaulting to none).

Examples of potential uses of these variables includes:

* `make CC="clang++ -fsanitize=address -fno-omit-frame-pointer" bin/incdb`. This
   uses LLVM's Clang++ compiler with the sanitize address option, to check for
   memory errors at runtime, and returns stack traces if violations are found.
* `make DEFINES=-DDEBUG`. This compiles all files with `#define DEBUG`. When
   compiled with this preprocessor macro defined, the resulting binary will
   print debug messages (For example, when running batches of updates, it will
   display which update it is currently serving, and some indication of how it
   is affecting the materialised view). Benchmark results will be skewed by
   these messages, so it is best not to compile with this flag enabled when the
   goal is to measure performance.

In both the above cases, it is wise to run `make clean` before the given
command, so that the effect is consistent across all compilation units.

This binary has been compiled and tested on the lab machines, as well as on Mac
OS X.

## Running

The binary is run on its own, simply by executing `bin/incdb` from the root of
the project. After it is run, the contents of the database file is left on disk
(by default under the filename "inc.db", in the working directory). `hexdump`
may be used to analyse the contents of this file to check the format in which
data is stored.

When `IncDB` is run, it will remove the old version of its database file, so, if
you wish to keep it, it must be moved before running the binary again.

## Setting Constants

Constants used throughout the database implementation may be found, and modified
at `include/dim.h`. These include:

* `NAME`, the name of the database file on disk (default: `"inc.db"`).
* `PAGE_SIZE`, The size of a single page, in bytes (default: `8 << 10 = 8KB`).
* `NUM_PAGES`, The number of available pages in the database file (default: `300000`).
* `POOL_SIZE`, The number of pages to hold resident in memory, in the buffer
   manager (default: `1000`).

These figures will result in a database file that is roughly 2.3GB large, and
approximately 8MB of RAM usage during the normal running of the database. These
figures were chosen as they are large enough to cope with the largest batch of
insertion transactions in the benchmark, but the smaller sets of transaction may
be run with far fewer available pages.

NB, the binary must be recompiled after changing these values.

## Creating New Queries

The process of setting up a query can be split up into four parts: Creating the
tables, loading the data, choosing the type of query, and running a set of
transactions. All changes that need to be made in creating a new query are
contained within `src/incdb.cpp`.

### Creating Tables

First an instance of `DB::Query::Tables` must be created. This is a map from
relation names (given as integers), to tables. Below is an example of an
instance that represents the tables (and the query variables) that represent the
triangle join (`R1(A, B) JOIN R2(A, C) JOIN R3(B, C)`):

    DB::Query::Tables R {
      {1, make_shared<DB::Table>(0, 1)},
      {2, make_shared<DB::Table>(0, 2)},
      {3, make_shared<DB::Table>(1, 2)},
    };

Each line in the initialisation corresponds to a single table. The first
parameter corresponds to the name of the relation (I.e. `1` denotes `R1`,
etc.). Within the second parameter, the arguments passed to `make_shared<>`
denote the query variables (i.e. `0 -> A, 1 -> B, 2 -> C`). When picking
integers to represent query parameters, it is important to ensure that they are
non-negative, and, if `[0, n]` is the smallest interval to cover all of them,
then for all integers `p` in `[0, n]`, `p` must be mentioned as a variable
somewhere in the query (In other words, no integers can be wasted). If these
conditions are not met, then the behaviour of the algorithm is undefined.

Below are some further initialisations of `R` and the joins they correspond to:

    // R1(A, B) JOIN R2(A, C)
    DB::Query::Tables R {
      {1, make_shared<DB::Table>(0, 1)},
      {2, make_shared<DB::Table>(0, 2)},
    };

    // R1(A, B) JOIN R2(A, C) JOIN R4(A, D)
    DB::Query::Tables R {
      {1, make_shared<DB::Table>(0, 1)},
      {2, make_shared<DB::Table>(0, 2)},
      {4, make_shared<DB::Table>(0, 3)},
    };

### Loading Data

Tables may be filled from CSV files by using the `DB::Table::loadFromFile`
instance method. Accessing an individual table by name is done using indexing
(provided it is initialised). Combining these ideas, to fill `R1` from file we
do the following:

    R[1]->loadFromFile("data/R1.txt");

### Choosing the Query

The query interface is implemented by four separate classes:

* `DB::NaiveCount`
* `DB::NaiveEquiJoin`
* `DB::IncrementalCount`
* `DB::IncrementalEquiJoin`

One of which must be instantiated (according to whether we wish to maintain the
count of the result, or the full join, and whether we wish to use the naive
update algorithm or the incremental update algorithm).

When instantiating one of these classes, we must provide two parameters:

* The size of a record in the join (The number of unique parameters in the query).
* The tables participating in the join.

After instantiation, it is important to call `DB::Query::recompute` on the new
query, to initialise the view with its current result, before any updates are
made.

### Running Transactions

It is possible to issue single updates to a query using the `DB::Query::update`
method which takes a table name, operation (`DB::Query::Insert` or
`DB::Query::Delete`), and record (as two separate parameters), and performs the
update whilst maintaining the query's view. For example `query.update(1,
DB::Query::Insert, 2, 3)` will insert `(2, 3)` into `R1` (whilst maintaining
`query`'s view).

Alternatively, a batch of transactions may be run, using the `DB::TestBed`
class, which reads transactions from a file, as follows:

    DB::TestBed tb(query);
    long time = tb.runFile(DB::Query::Insert, "data/I1.txt");

Both `DB::Query::update` and `DB::Testbed::runFile` return the cumulative time
taken to update the view, in microseconds.

### Further Information

Every header file is annotated with a brief description of the class being
defined, and interface documentation for every method.
