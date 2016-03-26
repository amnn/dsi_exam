#include "db.h"

#include "allocator.h"
#include "bufmgr.h"
#include "dim.h"

namespace DB {
  namespace Global {
    Allocator *ALLOC  = nullptr;
    BufMgr    *BUFMGR = nullptr;
  }
}
