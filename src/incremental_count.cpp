#include "incremental_count.h"

#include <iostream>
#include <memory>
#include <vector>

#include "leapfrog_triejoin.h"
#include "trie_iterator.h"

namespace DB {
  void
  IncrementalCount::recompute()
  {
    // Reset the counter
    mCount = 0;

    // Get fresh iterators.
    std::vector<std::unique_ptr<TrieIterator>> iters;
    for (const auto &tbl : getTables())
      iters.emplace_back(tbl->scan());

    // Build a Join from them.
    TrieIterator::Ptr query(new LeapFrogTrieJoin(getWidth(), move(iters)));

    TrieIterator::countingScan(query, mCount, getWidth());

#ifdef DEBUG
    std::cout << "|J| = " << mCount << std::endl;
#endif
  }

  void
  IncrementalCount::updateView(int table, Op op, int x, int y, bool didChange)
  {
    if (!didChange) {
#ifdef DEBUG
      std::cout << "|J| = " << mCount << "\t\t(*)" << std::endl;
#endif
      return;
    }

    // Get fresh iterators.
    int i = 1;
    std::vector<TrieIterator::Ptr> iters;
    for (const auto &tbl : getTables()) {
      // NB. Iterator for table that changed is a singleton.
      iters.emplace_back(i == table ? tbl->singleton(x, y) : tbl->scan());
      i++;
    }

    // Build a Join from them.
    TrieIterator::Ptr query(new LeapFrogTrieJoin(getWidth(), move(iters)));

    int delta = 0;
    TrieIterator::countingScan(query, delta, getWidth());

    switch (op) {
    case Query::Insert:
      mCount += delta;
      break;
    case Query::Delete:
      mCount -= delta;
      break;
    }

#ifdef DEBUG
    std::cout << "|J| = " << mCount
              << "\t\t(" << (op == Query::Insert ? '+' : '-')
              << delta << ")" << std::endl;
#endif
  }
}
