#ifndef DB_HEAP_FILE_H
#define DB_HEAP_FILE_H

#include <cstddef>

#include "allocator.h"
#include "dim.h"

namespace DB {
  /**
   * HeapFile
   *
   * Stores data sequentially. The interface is "write-only" and is used by the
   * NaiveDB incremental maintenance algorithm to simulate materialising the join.
   */
  struct HeapFile {
    /**
     * HeapFile::HeapFIle
     *
     * Construct an empty HeapFile.
     */
    HeapFile();

    /**
     * HeapFile::~HeapFile
     *
     * Free all the pages used by the HeapFile.
     */
    ~HeapFile();

    /**
     * HeapFile::append
     *
     * Add data to the end of the HeapFile.
     *
     * @param data Buffer to take data from;
     * @param len  Number of integers to copy.
     */
    void append(int *data, int len);

    /**
     * HeapFile::clear
     *
     * Free all but one page in the HeapFile. The one that is left is made
     * empty.
     */
    void clear();

  private:
    /**
     * (private) HeapFile::HeapPage
     *
     * Structure of an individual page in the HeapFile.
     */
    struct HeapPage {
      int count;
      page_id next;
      int data[1];
    };

    // How many integers can we fit in the data section of this page?
    static constexpr int PAGE_CAP =
      (Dim::PAGE_SIZE - offsetof(HeapPage, data)) / sizeof(int);

    HeapPage * mLastPage;
    page_id    mFirstPID;
    page_id    mLastPID;

    /**
     * (private) HeapFile::newPage
     *
     * @return the page ID of a fresh page initialised as an empty
     *         HeapPage. Note that this routine does not unpin the created page.
     */
    static page_id newPage(HeapPage *&hp);
  };
}

#endif // DB_HEAP_FILE_H
