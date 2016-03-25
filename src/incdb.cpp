#include <iostream>

#include "allocator.h"
#include "bufmgr.h"
#include "dim.h"

using namespace std;

int
main(int, char **)
{
  try {
    DB::Allocator a(DB::Dim::NAME, DB::Dim::PAGE_SIZE, DB::Dim::NUM_PAGES);

    DB::page_id pid = a.palloc(3);
    cout << a.spaceMap() << endl;

    a.pfree(pid);
    cout << a.spaceMap() << endl;

    pid = a.palloc(4);
    cout << a.spaceMap() << endl;

    a.pfree(pid, 2);
    cout << a.spaceMap() << endl;

    a.palloc(1); a.palloc(1);
    cout << a.spaceMap() << endl;

  } catch(exception &e) {
    cerr << "IncDB terminated due to exception: "
         << e.what() << endl;
  }
  return 0;
}
