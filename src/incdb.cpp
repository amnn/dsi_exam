#include <iostream>

#include "allocator.h"

using namespace std;

int
main(int, char **)
{
  DB::Allocator allocator("inc.db", 1 << 10, 1);

  cout << &allocator << endl;

  return 0;
}
