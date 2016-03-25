#include <string>
#include <vector>

namespace DB {
  /**
   * page_id
   * Type alias for page IDs
   */
  using page_id = unsigned;

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
     * Free the page, so that future allocations may use it. No check is made to
     * ensure the page is not already free.
     *
     * @param pid The page ID of the page to free.
     */
    void pfree(page_id pid);

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

  private:
    int mFD;                        // File descriptor for database file managed by this allocator.
    unsigned mPageSize;             // Number of bytes in a page.
    std::vector<bool> mSpaceMap;    // Map of free and occupied pages.
    std::string mName;              // Name of database file.
  };

}
