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
  };
}

#endif // DB_NAIVE_EQUIJOIN_H
