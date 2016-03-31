#include "naive_count.h"

#include <iostream>

#include "leapfrog_triejoin.h"
#include "trie_iterator.h"

namespace DB {
  void
  NaiveCount::recompute()
  {
    // Reset the counter
    mCount = 0;

    // Get fresh iterators.
    std::vector<std::unique_ptr<TrieIterator>> iters;
    for (const auto &tbl : getTables())
      iters.emplace_back(tbl->scan());

    // Build a Join query from them.
    std::unique_ptr<TrieIterator>
      query(new LeapFrogTrieJoin(getWidth(), move(iters)));

    // Count them
    scanCount(query);

#ifdef DEBUG
    std::cout << "|J| = " << mCount << std::endl;
#endif
  }

  void
  NaiveCount::scanCount(std::unique_ptr<TrieIterator> &it, int depth)
  {
    if (depth >= getWidth()) {
      mCount++;
      return;
    }

    it->open();
    while (!it->atEnd()) {
      scanCount(it, depth + 1);
      it->next();
    }
    it->up();
  }
}
