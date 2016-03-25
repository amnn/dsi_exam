#include <iostream>

#include "allocator.h"
#include "bufmgr.h"

using namespace std;

int
main(int, char **)
{
  try {
    const unsigned PAGE_SIZE = 1 << 10;
    DB::Allocator a("inc.db", PAGE_SIZE, 10);

    DB::page_id pid = a.palloc(1);

    char *buf = new char[PAGE_SIZE];
    *(int *)buf = 42;

    a.write(pid, buf);

    *(int *)buf = 0;
    a.read(pid, buf);

    cout << *(int *)buf << endl;

  } catch(exception &e) {
    cerr << "IncDB terminated due to exception: "
         << e.what() << endl;
  }
  return 0;
}
