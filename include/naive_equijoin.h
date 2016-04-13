#ifndef DB_NAIVE_EQUIJOIN_H
#define DB_NAIVE_EQUIJOIN_H

#include <memory>
#include <vector>

#include "heap_file.h"
#include "naive_query.h"
#include "table.h"
#include "trie_iterator.h"

namespace DB {
  /**
   * NaiveEquiJoin
   *
   * An implementation of naive incremental maintenance for equi-join queries.
   */
  struct NaiveEquiJoin : public NaiveQuery {
    /**
     * NaiveEquiJoin::NaiveEquiJoin
     *
     * Inherited from superclass.
     */
    using NaiveQuery::NaiveQuery;

    /**
     * NaiveEquiJoin::~NaiveEquiJoin
     *
     * Default virtual destructor.
     */
    ~NaiveEquiJoin() override = default;

    /** Query method overrides */
    void recompute() override;

  private:
    HeapFile mResult;
  };
}

#endif // DB_NAIVE_EQUIJOIN_H
