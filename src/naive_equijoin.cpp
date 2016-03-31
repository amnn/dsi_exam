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
    std::vector<std::unique_ptr<TrieIterator>> iters;
    for (const auto &tbl : getTables())
      iters.emplace_back(tbl->scan());

    // Build a Join query from them.
    std::unique_ptr<TrieIterator>
      query(new LeapFrogTrieJoin(getWidth(), move(iters)));

    // Pour it out into a result file.
    int *recBuf = new int[getWidth()]();
    scanIntoResult(query, recBuf);
    delete[] recBuf;
  }

  void
  NaiveEquiJoin::scanIntoResult(std::unique_ptr<TrieIterator> &it,
                                int *recBuf, int depth)
  {
    if (depth >= getWidth()) {

#ifdef DEBUG
      for (int i = 0; i < getWidth(); ++i)
        std::cout << recBuf[i] << " ";
      std::cout << std::endl;
#endif

      mResult.append(recBuf, getWidth());
      return;
    }

    it->open();
    while (!it->atEnd()) {
      recBuf[depth] = it->key();
      scanIntoResult(it, recBuf, depth + 1);
      it->next();
    }
    it->up();
  }
}
