#include "naive_equijoin.h"

#include <iostream>

#include "leapfrog_triejoin.h"
#include "trie_iterator.h"

namespace DB {
  void
  NaiveEquiJoin::recompute()
  {
    // Clear the current result.
    mResult.clear();

    // Get fresh iterators.
    std::vector<TrieIterator::Ptr> iters;
    for (const auto &kvp : getTables()) {
      const auto &tbl = std::get<1>(kvp);
      iters.emplace_back(tbl->scan());
    }

    // Build a Join query from them.
    TrieIterator::Ptr query(new LeapFrogTrieJoin(getWidth(), move(iters)));

    // Pour it out into a result file.
    int *recBuf = new int[getWidth()]();
    TrieIterator::traverse(query, getWidth(), recBuf, [this, recBuf] {
#ifdef DEBUG
        for (int i = 0; i < getWidth(); ++i)
          std::cout << recBuf[i] << " ";
        std::cout << std::endl;
#endif

        mResult.append(recBuf, getWidth());
      });
    delete[] recBuf;
  }
}
