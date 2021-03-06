#ifndef DB_ALLOCATOR_H
#define DB_ALLOCATOR_H

#include <string>
#include <vector>

namespace DB {
  /**
   * page_id
   * Type alias for page IDs
   */
  using page_id = unsigned;
  static constexpr page_id INVALID_PAGE = -1;

  /**
   * Allocator
   * Manages database files, and allocates pages from them.
   */
  struct Allocator {
    /**
     * Allocator::Allocator
     *
     * Create a new page allocator.
     *
     * @param fname    Name of database file to open (cannot already exist).
     * @param psize    Size of a page, in bytes
     * @param pcount   Number of pages to reserve in file.
     */
    Allocator(const char *fname, unsigned psize, unsigned pcount);

    /**
     * Allocator::~Allocator
     *
     * Clean up the file being managed by this allocator (close the file
     * descriptor, and unlink the file).
     */
    ~Allocator();

    /** Allocators cannot be copied */
    Allocator(const Allocator &) = delete;
    Allocator &operator =(const Allocator &) = delete;

    /**
     * Allocator::palloc
     *
     * Allocate a run of pages with contiguous IDs.
     *
     * @param  num The number of pages to allocate
     * @return The page ID of the first page in the run.
     */
    page_id palloc(unsigned num);

    /**
     * Allocator::pfree
     *
     * Free a run of pages, so that future allocations may use them. No check is
     * made to ensure they are not already free.
     *
     * @param pid The page ID of the page to free.
     * @param num The number of consecutive pages after this one to
     *            free. (Defaults to 1)
     */
    void pfree(page_id pid, int num = 1);

    /**
     * Allocator::read
     *
     * Read the contents of a page into buf. The caller must ensure that buf is
     * wide enough to contain the entire page, and that pid points to a valid
     * page. No check is performed to ensure that the page is allocated.
     *
     * @param pid The page ID of the page to read.
     * @param buf The buffer to put the contents of the page into.
     */
    void read(page_id pid, char *buf);

    /**
     * Allocator::write
     *
     * Write the contents of buf into a page. The caller must ensure that buf is
     * atleast as wide as a page, and that pid points to a valid page. No check
     * is performed to ensure that the page is allocated.
     *
     * @param pid The page ID of the page to write to.
     * @param buf The buffer of content to write to the page.
     */
    void write(page_id pid, char *buf);

    /**
     * Allocator::spaceMap
     *
     * A representation of the space map as a string of 1's and 0's. 1
     * representing an allocated page, and 0 a free page.
     *
     * @return a string representation of the space map.
     */
    std::string spaceMap() const;
  private:
    int mFD;                        // File descriptor for database file managed by this allocator.
    unsigned mPageSize;             // Number of bytes in a page.
    std::vector<bool> mSpaceMap;    // Map of free and occupied pages.
    std::string mName;              // Name of database file.
  };

}

#endif // DB_ALLOCATOR_H
