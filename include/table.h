#ifndef DB_TABLE_H
#define DB_TABLE_H

#include <cstddef>

#include <memory>

#include "allocator.h"
#include "dim.h"
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
     * Table::loadFromFile
     *
     * Insert data into the table from a file. The file should be
     * reaad-accessible to the database, and the format should be CSV with one
     * record (two columns) per line.
     *
     * @param fname The name of the file to load from
     */
    void loadFromFile(const char *fname);

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
     * Table::remove
     *
     * Remove a record from the table.
     *
     * @param x The value of the record's first column.
     * @param y The value of the record's second column.
     * @return True iff the deletion changed the table.
     */
    bool remove(int x, int y);

    /**
     * Table::scan
     *
     * @return A pointer to an iterator that can traverse the contents of the
     * table. The iterator moves in only one direction, and its behaviour is
     * undefined if the table is modified whilst it is in use.
     */
    TrieIterator::Ptr scan();

    /**
     * Table::singleton
     *
     * Produce an iterator for a one record slice of the table.
     *
     * @param x The value of the record's first column.
     * @param y The value of the record's second column.

     * @return An iterator containing just the record [x, y] as if it originated
     *         from an iterator for this table.
     */
    TrieIterator::Ptr singleton(int x, int y);

  private:

    page_id mRootPID;
    int     mRootOrder;
    int     mSubOrder;
    bool    mIsReversed;
  };
}

#endif // DB_TABLE_H
