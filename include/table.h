#include "allocator.h"
#include "trie_iterator.h"

namespace DB {
  /**
   * Table
   *
   * Representation of input tables, stored in a Nested B+ Trie. It is assumed
   * that all input tables have 2 integer columns, and do not permit duplicates.
   */
  struct Table {

    /**
     * Table::Table
     *
     * Constructs an empty table.
     *
     * @param order1 The position of the table's first column in the global
     *               ordering.
     * @param order2 The position of the table's second column in the global
     *               ordering.
     */
    Table(int order1, int order2);

    /**
     * Table::insert
     *
     * Insert a record into the table.
     *
     * @param x The value of the record's first column.
     * @param y The value of the record's second column.
     * @return True iff the insertion changed the table.
     */
    bool insert(int x, int y);

    /**
     * Table::insert
     *
     * Remove a record from the table.
     *
     * @param x The value of the record's first column.
     * @param y The value of the record's second column.
     * @return True iff the deletion changed the table.
     */
    bool delete(int x, int y);

    /**
     * Table::scan
     *
     * @return An iterator that can traverse the contents of the table. The
     * iterator moves in only one direction, and its behaviour is undefined if
     * the table is modified whilst it is in use.
     */
    TrieIterator scan();

  private:
    page_id mRootPID;
    int     mOrder1;
    int     mOrder2;
  };
}
