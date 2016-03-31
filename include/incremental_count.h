#ifndef DB_INCREMENTAL_COUNT_H
#define DB_INCREMENTAL_COUNT_H

#include "query.h"

namespace DB {
  struct IncrementalCount : public Query {
    /**
     * IncrementalCount::IncrementalCount
     *
     * Inherited from superclass.
     */
    using Query::Query;

    /**
     * IncrementalCount::~IncrementalCount
     *
     * Default overriden virtual destructor.
     */
    ~IncrementalCount() override = default;

    /** Query method overrides */
    void recompute() override;

  protected:
    void updateView(int table, Op op, int x, int y, bool didChange) override;

  private:
    int mCount;
  };
}

#endif // DB_INCREMENTAL_COUNT_H
