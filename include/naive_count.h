#ifndef DB_NAIVE_COUNT_H
#define DB_NAIVE_COUNT_H

#include <memory>
#include <vector>

#include "heap_file.h"
#include "naive_query.h"
#include "table.h"
#include "trie_iterator.h"

namespace DB {
  /**
   * NaiveCount
   *
   * An implementation of naive incremental maintenance for count queries.
   */
  struct NaiveCount : public NaiveQuery {
    /**
     * NaiveCount::NaiveCount
     *
     * Inherited from superclass.
     */
    using NaiveQuery::NaiveQuery;

    /**
     * NaiveCount::~NaiveCount
     *
     * Default virtual destructor.
     */
    ~NaiveCount() override = default;

    /** Query method overrides */
    void recompute() override;

  private:
    int mCount;

    /**
     * (private) NaiveCount::scanCount
     *
     * Count the number of records in the iterator by scanning through it.
     *
     * @param it     The iterator to scan through.
     * @param depth  The depth to start filling from (defaults to 0).
     */
    void scanCount(TrieIterator::Ptr &it, int depth = 0);
  };
}

#endif // DB_NAIVE_EQUIJOIN_H
