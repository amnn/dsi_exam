#include "naive_count.h"

#include <iostream>
#include <memory>
#include <vector>

#include "leapfrog_triejoin.h"
#include "trie_iterator.h"

namespace DB {
  void
  NaiveCount::recompute()
  {
    // Reset the counter
    mCount = 0;

    // Get fresh iterators.
    std::vector<TrieIterator::Ptr> iters;
    for (const auto &tbl : getTables())
      iters.emplace_back(tbl->scan());

    // Build a Join query from them.
    std::unique_ptr<TrieIterator>
      query(new LeapFrogTrieJoin(getWidth(), move(iters)));

    // Count them
    TrieIterator::countingScan(query, mCount, getWidth());

#ifdef DEBUG
    std::cout << "|J| = " << mCount << std::endl;
#endif
  }
}
