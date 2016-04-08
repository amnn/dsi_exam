#include <iostream>
#include <memory>
#include <vector>

#include "allocator.h"
#include "bufmgr.h"
#include "db.h"
#include "dim.h"
#include "incremental_count.h"
#include "incremental_equijoin.h"
#include "naive_count.h"
#include "naive_equijoin.h"
#include "table.h"
#include "test_bed.h"
#include "view.h"

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

    DB::Query::Tables R {
      {1, make_shared<DB::Table>(0, 1)},
      {2, make_shared<DB::Table>(0, 2)}
    };

    R[1]->loadFromFile("data/R1.txt");
    R[2]->loadFromFile("data/R2.txt");

    DB::IncrementalEquiJoin query(3, R);
    query.recompute();

    DB::TestBed tb(query);
    long time = tb.runFile(DB::Query::Delete, "data/D5.txt");
    cout << time << " us elapsed." << endl;

  } catch(exception &e){
    cerr << "\n\nIncDB terminated due to exception: "
         << e.what() << endl;
  }
  return 0;
}
