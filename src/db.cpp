#include "db.h"

#include "allocator.h"
#include "bufmgr.h"
#include "dim.h"

namespace DB {
  namespace Global {
    Allocator *ALLOC  = nullptr;
    BufMgr    *BUFMGR = nullptr;

    void
    setup() {
      ALLOC = new Allocator(Dim::NAME,
                            Dim::PAGE_SIZE,
                            Dim::NUM_PAGES);

      BUFMGR = new BufMgr(Dim::POOL_SIZE);
    }

    void
    tearDown() {
      delete ALLOC;  ALLOC  = nullptr;
      delete BUFMGR; BUFMGR = nullptr;
    }
  }
}
