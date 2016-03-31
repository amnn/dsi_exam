#include "trie_iterator.h"

namespace DB {
  void
  TrieIterator::countingScan(Ptr &it, int &count, int depth)
  {
    if (depth <= 0) {
      count++;
      return;
    }

    it->open();
    while (!it->atEnd()) {
      countingScan(it, count, depth - 1);
      it->next();
    }
    it->up();
  }

}
