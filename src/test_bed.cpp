#include "test_bed.h"

#include <iostream>
#include <fstream>

#include "query.h"

namespace DB {
  TestBed::TestBed(Query &query)
    : mQuery { query }
  {}

  long
  TestBed::runFile(Query::Op op, const char *fname)
  {
    std::ifstream file(fname);

    long elapsed = 0;

    int tn, x, y; char c1, c2;
    while ((file >> tn >> c1 >> x >> c2 >> y) &&
           (c1 == ',')                        &&
           (c2 == ',')) {

#ifdef DEBUG
      std::cout << (op == Query::Insert ? '+' : '-')
                << "R" << tn << "(" << x << "," << y << ")"
                << std::endl;
#endif

      elapsed += mQuery.update(tn, op, x, y);
    }

    return elapsed;
  }
}
