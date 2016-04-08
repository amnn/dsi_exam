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

  void
  TrieIterator::traverse(Ptr &it, int depth, int *rec,
                         std::function<void(void)> act,
                         int pos)
  {
    if (pos >= depth) {
      act();
      return;
    }

    it->open();
    while (!it->atEnd()) {
      rec[pos] = it->key();
      traverse(it, depth, rec, act, pos + 1);
      it->next();
    }
    it->up();
  }
}
