#ifndef DB_QUERY_H
#define DB_QUERY_H

#include "table.h"

namespace DB {
  /**
   * Query
   *
   * Abstract class for implementing equijoin and count queries with incremental
   * maintenance. This class deals with keeping views in sync with input tables,
   * and timing how long updates take.
   */
  struct Query {
    /**
     * Query::Tables
     *
     * Alias for a sequence of tables.
     */
    using Tables = std::vector<std::shared_ptr<Table>>;

    /**
     * Query::Op
     *
     * Tag for update operations.
     */
    enum Op : unsigned char { Insert, Delete };

    /**
     * Query::Query
     *
     * Constructor for queries, remembers the input tables for the query and the
     * width of output records.
     *
     * @param width  Size of a record in the join (The number of unique
     *               parameters in the query).
     * @param tables The list of tables to keep track of.
     */
    Query(int width, Tables tables);

    /**
     * Query::~Query
     *
     * Default virtual destructor.
     */
    virtual ~Query() = default;

    /**
     * Query::update
     *
     * Perform an update on the given table, and update the result of the query
     * to reflect this change.
     *
     * @param table The index of the table to update. This number is 1-indexed,
     *              and refers to the order in which tables were given to the
     *              query when it was constructed.
     * @param op   The operation to perform.
     * @param x    The value of the first column of the record.
     * @param y    The value of the second column of the record.
     * @return The time in milliseconds required to update the view.
     */
    long update(int table, Op op, int x, int y);

    /**
     * Query::recompute
     *
     * Force the query to completely recompute its view.
     */
    virtual void recompute() = 0;

  protected:
    /**
     * (protected) Query::updateView
     *
     * Update the view to reflect the fact the change to the input
     * table. Concrete sub-classes must implement this.
     *
     * @param table The index of the table to update. This number is 1-indexed,
     *              and refers to the order in which tables were given to the
     *              query when it was constructed.
     * @param op The operation to perform.
     * @param x The value of the first column of the record.
     * @param y The value of the second column of the record.
     * @param didChange True iff the input table changed.
     */
    virtual void updateView(int table, Op op, int x, int y, bool didChange) = 0;

    /**
     * (protected) Query::getTables
     *
     * @return A (const) reference to the sequence of tables
     */
    const Tables &getTables() const;

    /**
     * (protected) Query::getWidth()
     *
     * @return The width of a single record in the output of the join.
     */
    int getWidth() const;

  private:
    int    mWidth;
    Tables mTables;
  };
}

#endif // DB_QUERY_H
