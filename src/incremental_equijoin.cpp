#include "incremental_equijoin.h"

#include <iostream>
#include <memory>
#include <tuple>
#include <vector>

#include "leapfrog_triejoin.h"
#include "trie_iterator.h"

namespace DB {
  IncrementalEquiJoin::IncrementalEquiJoin(int width, Tables tables)
    : Query(width, move(tables))
    , mJoin (width)
  {}

  void
  IncrementalEquiJoin::recompute()
  {
    // Reset the table (doesn't actually do anything).
    mJoin.clear();

    // Get fresh iterators.
    std::vector<TrieIterator::Ptr> iters;
    for (const auto &kvp : getTables()) {
      const auto &tbl = std::get<1>(kvp);
      iters.emplace_back(tbl->scan());
    }

    // Build a Join from them, and count the records in it.
    TrieIterator::Ptr query(new LeapFrogTrieJoin(getWidth(), move(iters)));

    int *rec = new int[getWidth()]();
    TrieIterator::traverse(query, getWidth(), rec, [this, rec]() {
        mJoin.insert(rec);
      });
    delete[] rec;
  }

  void
  IncrementalEquiJoin::updateView(int table, Op op, int x, int y, bool didChange)
  {
    if (!didChange) {
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

    int txnsLogged = 0; // Used only in Debug mode.
    int *rec = new int[getWidth()]();
    TrieIterator::traverse(query, getWidth(), rec,
                           [this, op, rec, &txnsLogged]() {

#ifdef DEBUG
                             if (txnsLogged++ / 100 == 0)
                               std::cout << "." << std::flush;
#endif

                             switch (op) {
                             case Insert:
                               mJoin.insert(rec);
                               break;
                             case Delete:
                               mJoin.remove(rec);
                               break;
                             }
                           });
    delete[] rec;

#ifdef DEBUG
    std::cout << std::endl;
#endif
  }
}
