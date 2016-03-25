#include <iostream>

#include "allocator.h"

using namespace std;

int
main(int, char **)
{
  DB::Allocator allocator("inc.db", 0, 0);

  cout << &allocator << endl;

  return 0;
}
