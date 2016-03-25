#include "allocator.h"

namespace DB {
  Allocator::Allocator(const char * fname,
                       size_t       psize,
                       int          pcount)
  {
  }

  Allocator::~Allocator()
  {
  }

  page_id
  Allocator::palloc(int num)
  {
    return 0;
  }

  void
  read(page_id pid, char *buf)
  {
  }

  void
  write(page_id pid, char *buf)
  {
  }
}
