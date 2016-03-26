#include <iostream>

#include "allocator.h"
#include "bufmgr.h"
#include "db.h"

using namespace std;

int
main(int, char **)
{
  try {
    DB::Global::setup();

    DB::Allocator *a = DB::Global::ALLOC;
    DB::BufMgr    *b = DB::Global::BUFMGR;

    char *page;

    DB::page_id pid = b->bnew(page, 3);
    cout << a->spaceMap() << endl;

    b->bfree(pid + 1);
    cout << a->spaceMap() << endl;

    b->unpin(pid);
    pid = b->bnew(page, 4);
    cout << a->spaceMap() << endl;

    b->unpin(pid);
    b->flush(pid);
    a->pfree(pid, 2);
    cout << a->spaceMap() << endl;

    b->bnew(page); b->bnew(page);
    cout << a->spaceMap() << endl;

    DB::Global::tearDown();

  } catch(exception &e) {
    cerr << "IncDB terminated due to exception: "
         << e.what() << endl;
  }
  return 0;
}
