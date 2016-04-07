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
#include "test_bed.h"
#include "trie_iterator.h"
#include "view.h"

using namespace std;

void scanByLine(DB::TrieIterator::Ptr &it,
                int depth, int *rec,
                int stride, int &count);

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

    /*
    DB::Query::Tables R {
      {1, make_shared<DB::Table>(0, 1)},
      {2, make_shared<DB::Table>(0, 2)},
      {4, make_shared<DB::Table>(0, 3)}
    };

    R[1]->loadFromFile("data/R1.txt");
    R[2]->loadFromFile("data/R2.txt");
    R[4]->loadFromFile("data/R4.txt");

    DB::IncrementalCount query(4, R);
    query.recompute();

    DB::TestBed tb(query);
    long time = tb.runFile(DB::Query::Delete, "data/D4.txt");
    cout << time << " us elapsed." << endl;
    */

    DB::View v(1);

    for (int i = 1; i <= 56; ++i)
      v.insert(&i);

    for (;;) {
      char c; int x;
      cin >> c >> x;
      if      (c == '+') v.insert(&x);
      else if (c == '-') v.remove(&x);
      else if (c == '!') break;
    }

  } catch(exception &e) {
    cerr << "\n\nIncDB terminated due to exception: "
         << e.what() << endl;
  }
  return 0;
}

void
scanByLine(DB::TrieIterator::Ptr &it,
           int depth, int *rec,
           int stride, int &count)
{
  if (depth >= stride) {
    for (int i = 0; i < stride; ++i)
      cout << rec[i] << " ";
    cout << endl;
    count++;
    return;
  }

  it->open();
  while (!it->atEnd()) {
    rec[depth] = it->key();
    scanByLine(it, depth + 1, rec, stride, count);
    it->next();
  }
  it->up();
}
