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

    cout << a.spaceMap() << endl;

    cout << "I1...";
    for (int i = 0; i < 32; ++i) {
      if (!t1.insert(i + 1, 1))
        cout << "!I " << i + 1 << endl;
    }
    cout << "Done." << endl;
    cout << a.spaceMap() << endl;

    cout << "R1...";
    for (int i = 31; i >= 0; --i) {
      if (!t1.remove(i + 1, 1))
        cout << "!R " << i + 1 << endl;
    }
    cout << "Done." << endl;
    cout << a.spaceMap() << endl;

    cout << "R2...";
    for (int i = 0; i < 32; ++i) {
      if (t1.remove(i + 1, 1))
        cout << "!R " << i + 1 << endl;
    }
    cout << "Done." << endl;
    cout << a.spaceMap() << endl;

    cout << "I2...";
    for (int i = 31; i >= 0; --i) {
      if (!t1.insert(i + 1, 1))
        cout << "!I " << i + 1 << endl;
    }
    cout << "Done." << endl;
    cout << a.spaceMap() << endl;

    cout << "R3...";
    for (int i = 31; i >= 0; --i) {
      if (!t1.remove(i + 1, 1))
        cout << "!R " << i + 1 << endl;
    }
    cout << "Done." << endl;
    cout << a.spaceMap() << endl;

    cout << "R4...";
    for (int i = 0; i < 32; ++i) {
      if (t1.remove(i + 1, 1))
        cout << "!R " << i + 1 << endl;
    }
    cout << "Done." << endl;
    cout << a.spaceMap() << endl;

    cout << "I3...";
    for (int i = 0; i < 32; ++i) {
      if (!t1.insert(i + 1, 1))
        cout << "!I " << i + 1 << endl;
    }
    cout << "Done." << endl;
    cout << a.spaceMap() << endl;

    cout << "I4...";
    for (int i = 0; i < 32; ++i) {
      if (t1.insert(i + 1, 1))
        cout << "!I " << i + 1 << endl;
    }
    cout << "Done." << endl;
    cout << a.spaceMap() << endl;

    /**
    */

  } catch(exception &e) {
    cerr << "IncDB terminated due to exception: "
         << e.what() << endl;
  }
  return 0;
}
