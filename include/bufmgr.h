#ifndef DB_BUFMGR_H
#define DB_BUFMGR_H

#include "allocator.h"
#include "frame.h"
#include "replacer.h"

namespace DB {
  /**
   * BufMgr
   *
   * Manages a cache of pages from the database file that are resident in main
   * memory.
   */
  struct BufMgr {
    /**
     * BufMgr::BufMgr
     *
     * Constructs a new buffer manager in which all the frames are empty.
     *
     * @param poolSize The number of frames in this buffer pool.
     */
    BufMgr(int poolSize);

    /**
     * BufMgr::~BufMgr
     *
     * Destructor for buffer manager. Evicts all resident pages.
     */
    ~BufMgr();

    /**
     * BufMgr::pin
     *
     * Bring the page with page id = pid into the buffer pool. If isEmpty is
     * set, the frame it resides in is simply cleared, otherwise, the page is
     * read in from file.
     *
     * @param pid The page ID to pin
     * @param isEmpty A flag to determine whether reading from file is necessary
     * @return If the page is successfully pinned, a pointer to its data is
     *         returned, and if not, nullptr is returned.
     */
    char *pin(page_id pid, bool isEmpty);

    /**
     * BufMgr::unpin
     *
     * Decrease the pin count on the page. If the pin count becomes 0, the page
     * can be evicted. It is an error to call this function on pages that are
     * not currently resident in the pool.
     *
     * @param pid   The page ID to be unpinned
     * @param dirty flag to indicate whether the page has been written to,
     *              defaults to false.
     */
    void unpin(page_id pid, bool dirty = false);

    /**
     * BufMgr::bnew
     *
     * Allocate a string of contiguous pages, and pin the first page.
     *
     * @param first A reference that is populated with the pointer to the first
     *              page's data if the operation was a success, and with nullptr
     *              otherwise.
     *
     * @param howMany The number of pages to allocate, defaults to 1.
     *
     * @return The page ID of the first page allocated, if the operation was
     *         successful, and INVALID_PAGE otherwise.
     */
    page_id bnew(char *&first, int howMany = 1);

    /**
     * BufMgr::bfree
     *
     * Free the memory allocated for the page in the buffer, and deallocate it
     * on file. This operation cannot be performed if the pin count on the page
     * is greater than 0.
     *
     * @param pid The page ID of the page to free.
     */
    void bfree(page_id pid);

    /**
     * BufMgr::flush
     *
     * Commit the contents of the page with the given ID back to the file, so
     * long as the page is in the buffer pool, and is not pinned.
     *
     * @param pid The page ID of the page to be flushed.
     */
    void flush(page_id pid);

  private:
    Frame *  mFrames;
    Replacer mReplacer;
    int      mPoolSize;

    /**
     * (private) BufMgr::findFrame
     *
     * Search for a frame in the buffer pool that could contain the given page.
     *
     * @param pid the page ID to find
     * @return If the page is already in the pool, then return the frame it is
     *         in, otherwise, if there is an empty frame, we return its
     *         index. Failing that, we return INVALID_FRAME.
     */
    int findFrame(page_id pid);
  };
}

#endif // DB_BUFMGR_H
