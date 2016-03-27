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

    DB::Table t1(0, 1);

    cout << "Inserting...";
    for(int i = 0; i < 32; ++i) {
      if (!t1.insert(i + 1, 1))
        cout << "Hmm, this should be true" << endl;
    }
    cout << "Done." << endl;

    cout << "Re-inserting...";
    for(int i = 0; i < 32; ++i) {
      if (t1.insert(i + 1, 1)) {
        cout << "Hmm, I've already inserted " << i + 1 << endl;
      }
    }
    cout << "Done." << endl;

  } catch(exception &e) {
    cerr << "IncDB terminated due to exception: "
         << e.what() << endl;
  }
  return 0;
}
