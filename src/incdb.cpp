#include <iostream>

#include "allocator.h"

using namespace std;

int
main(int, char **)
{
  try {
    DB::Allocator allocator("inc.db", 1 << 10, 1);
    cout << &allocator << endl;

    allocator.palloc(2);

  } catch(exception &e) {
    cerr << "IncDB terminated due to exception: "
         << e.what() << endl;
  }
  return 0;
}
