#ifndef DB_VIEW_H
#define DB_VIEW_H

#include "allocator.h"
#include "ftree.h"

namespace DB {
  /**
   * View
   *
   * Representation of output tables, stored in a Nested Fractal Trie.
   */
  struct View {
    /**
     * View::View
     *
     * Construct an empty table where each record is `width` columns wide.
     *
     * @param width The number of columns in the view.
     */
    View(int width);

    /**
     * View::~View
     *
     * Destructor, unpins the root node.
     */
    ~View();

    /**
     * View::insert
     *
     * Add a new record to the table.
     *
     * @param data A buffer holding the contents of the record. It is assumed
     *             that there are atleast as many integers in the buffer as the
     *             records are wide in this View.
     */
    void insert(int *data);

    /**
     * View::remove
     *
     * Remove a record in the table.
     *
     * @param data A buffer holding the contents of the record. It is assumed
     *             that there are atleast as many integers in the buffer as the
     *             records are wide in this View.
     */
    void remove(int *data);

    /**
     * View::clear
     *
     * Remove *all* the records from this view. Unlike insertions and removals
     * of single elements, this operation is performed eagerly, freeing all the
     * pages used by the view.
     */
    void clear();

  private:
    int     mWidth;
    page_id mRootPID;

    // The root node is pinned to make batches of insertions faster:
    FTree * mTree;

    /**
     * (private) View::logTxn
     *
     * Log a transaction in the Fractal Trie, and maintain its structure in the
     * presence of splits and merges.
     *
     * @param msg  The transaction type.
     * @param data The associated data buffer.
     */
    void logTxn(FTree::TxnType msg, int *data);
  };
}

#endif // DB_VIEW_H
