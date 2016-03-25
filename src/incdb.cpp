#include <iostream>

#include "allocator.h"
#include "db.h"

using namespace std;

int
main(int, char **)
{
  try {
    DB::Global::setup();

    DB::Allocator *a = DB::Global::ALLOC;

    DB::page_id pid = a->palloc(3);
    cout << a->spaceMap() << endl;

    a->pfree(pid);
    cout << a->spaceMap() << endl;

    pid = a->palloc(4);
    cout << a->spaceMap() << endl;

    a->pfree(pid, 2);
    cout << a->spaceMap() << endl;

    a->palloc(1); a->palloc(1);
    cout << a->spaceMap() << endl;

    DB::Global::tearDown();

  } catch(exception &e) {
    cerr << "IncDB terminated due to exception: "
         << e.what() << endl;
  }
  return 0;
}
