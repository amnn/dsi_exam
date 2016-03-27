#include <iostream>

#include "allocator.h"
#include "bufmgr.h"
#include "db.h"
#include "table.h"
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

    cout << a.spaceMap() << endl;

    DB::Table t1(0, 1);

    for(int i = 0; i < 100000; ++i) {
      // cout << "Inserting " << i + 1 << endl;
      t1.insert(i/100, i);
    }

    cout << a.spaceMap() << endl;
  } catch(exception &e) {
    cerr << "IncDB terminated due to exception: "
         << e.what() << endl;
  }
  return 0;
}
