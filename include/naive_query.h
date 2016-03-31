#ifndef DB_NAIVE_QUERY_H
#define DB_NAIVE_QUERY_H

#include "query.h"

namespace DB {
  /**
   * NaiveQuery
   *
   * Thin wrapper around Query that defines `updateView` in terms of
   * `recompute`.
   */
  struct NaiveQuery : public Query {
    using Query::Query;

    void updateView(int, Op, int, int, bool) override
    {
      recompute();
    }
  };
}

#endif // DB_NAIVE_QUERY_H
