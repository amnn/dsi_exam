#ifndef DB_INCREMENTAL_EQUIJOIN_H
#define DB_INCREMENTAL_EQUIJOIN_H

#include "query.h"
#include "view.h"

namespace DB {
  struct IncrementalEquiJoin : public Query {
    /**
     * IncrementalEquiJoin::IncrementalEquiJoin
     *
     * Constructor.
     */
    IncrementalEquiJoin(int width, Tables tables);

    /**
     * IncrementalEquiJoin::IncrementalEquiJoin
     *
     * Default overriden virtual destructor.
     */
    ~IncrementalEquiJoin() override = default;

    /** Query method overrides */
    void recompute() override;

  protected:
    void updateView(int table, Op op, int x, int y, bool didChange) override;

  private:
    View mJoin;
  };
}

#endif // DB_INCREMENTAL_EQUIJOIN_H
