#include "incremental_count.h"

#include <iostream>
#include <memory>
#include <tuple>
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
    std::vector<TrieIterator::Ptr> iters;
    for (const auto &kvp : getTables()) {
      const auto &tbl = std::get<1>(kvp);
      iters.emplace_back(tbl->scan());
    }

    // Build a Join from them, and count the records in it.
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
    std::vector<TrieIterator::Ptr> iters;
    for (const auto &kvp : getTables()) {
      auto k          = std::get<0>(kvp);
      const auto &tbl = std::get<1>(kvp);

      // NB. Iterator for table that changed is a singleton.
      iters.emplace_back(table == k ? tbl->singleton(x, y) : tbl->scan());
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
