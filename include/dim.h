#ifndef DB_DIM_H
#define DB_DIM_H

namespace DB {
  /**
   * Dim
   *
   * A centralised location for global constants used throughout the database.
   */
  namespace Dim {
    constexpr const char *NAME = "inc.db";

    constexpr unsigned PAGE_SIZE = 8 << 10;
    constexpr unsigned NUM_PAGES = 300000;
    constexpr unsigned POOL_SIZE = 1000;
  }
}

#endif // DB_DIM_H
