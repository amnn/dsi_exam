#include "query.h"

#include <chrono>

namespace DB {
  Query::Query(int width, Tables tables)
    : mWidth  ( width )
    , mTables ( std::move(tables) )
  {}

  long
  Query::update(int table, Op op, int x, int y)
  {
    bool didChange;

    auto it = mTables.find(table);
    if (it == mTables.end()) {
      // The table isn't actually part of this join, we're going to ignore it.
      didChange = false;
    } else {
      switch (op) {
      case Query::Insert:
        didChange = mTables[table]->insert(x, y);
        break;
      case Query::Delete:
        didChange = mTables[table]->remove(x, y);
        break;
      }
    }

    using ns    = std::chrono::nanoseconds;
    using clock = std::chrono::steady_clock;
    auto begin  = clock::now();

    // Update the view.
    updateView(table, op, x, y, didChange);

    // Calculate Time taken.
    return std::chrono::duration_cast<ns>(clock::now() - begin).count();
  }

  const Query::Tables &
  Query::getTables() const
  {
    return mTables;
  }

  int
  Query::getWidth() const
  {
    return mWidth;
  }
}
