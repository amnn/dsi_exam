#ifndef DB_DB_H
#define DB_DB_H

#include "allocator.h"
#include "bufmgr.h"
#include "dim.h"

namespace DB {
  /**
   * Global instances shared across the database, and their associated setup and
   * teardown functions.
   */
  namespace Global {
    extern Allocator *ALLOC;
    extern BufMgr *BUFMGR;

    void setup();
    void tearDown();
  }
}

#endif // DB_DB_H
