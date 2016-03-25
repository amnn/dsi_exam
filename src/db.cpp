#include "db.h"

#include "allocator.h"
#include "dim.h"

namespace DB {
  namespace Global {
    Allocator *ALLOC = nullptr;

    void
    setup() {
      ALLOC = new Allocator(Dim::NAME,
                            Dim::PAGE_SIZE,
                            Dim::NUM_PAGES);
    }

    void
    tearDown() {
      delete ALLOC; ALLOC = nullptr;
    }
  }
}
