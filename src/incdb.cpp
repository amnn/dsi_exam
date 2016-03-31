#include <iostream>
#include <memory>
#include <vector>

#include "allocator.h"
#include "bufmgr.h"
#include "db.h"
#include "dim.h"
#include "incremental_count.h"
#include "leapfrog_triejoin.h"
#include "naive_count.h"
#include "table.h"
#include "trie_iterator.h"

using namespace std;

int
main(int, char **)
{
  try {
    DB::Allocator a(DB::Dim::NAME,
                    DB::Dim::PAGE_SIZE,
                    DB::Dim::NUM_PAGES);

    DB::BufMgr    b(DB::Dim::POOL_SIZE);

    DB::Global::ALLOC  = &a;
    DB::Global::BUFMGR = &b;

    auto
      R1 = make_shared<DB::Table>(0, 1),
      R2 = make_shared<DB::Table>(0, 2),
      R4 = make_shared<DB::Table>(0, 3);

    R1->loadFromFile("data/R1.txt");
    R2->loadFromFile("data/R2.txt");
    R4->loadFromFile("data/R4.txt");

    DB::IncrementalCount join(4, {R1, R2, R4});

    join.recompute();
    join.update(1, DB::Query::Insert, 1, 42);
    join.update(1, DB::Query::Insert, 1, 42);
    join.update(1, DB::Query::Delete, 1, 42);
    join.update(1, DB::Query::Delete, 1, 42);

  } catch(exception &e) {
    cerr << "\n\nIncDB terminated due to exception: "
         << e.what() << endl;
  }
  return 0;
}
