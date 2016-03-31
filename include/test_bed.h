#ifndef DB_TEST_BED_H
#define DB_TEST_BED_H

#include "query.h"

namespace DB {
  /**
   * TestBed
   *
   * Reads batches of instructions from a file, and runs them on a query,
   * keeping track of the cumulative time taken to update the view.
   */
  struct TestBed {
    /**
     * TestBed::TestBed
     *
     * Construct the test bed.
     *
     * @param &query The query to feed updates to.
     */
    TestBed(Query &query);

    /**
     * TestBed::runFile
     *
     * @param op The operation to treat it transaction as.
     * @param fname The name of the file holding the transactions, in a CSV
     *              format with one transaction on every line. Each transaction
     *              is a table "name" (a number), followed by the transaction's
     *              record.
     * @return The time elapsed in updating the view whilst responding to the
     *         transactions in milliseconds.
     */
    long runFile(Query::Op op, const char *fname);

  private:
    Query &mQuery;
  };
}

#endif // DB_TEST_BED_H
