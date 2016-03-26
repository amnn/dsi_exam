#ifndef DB_FRAME_H
#define DB_FRAME_H

#include "allocator.h"
#include "dim.h"

namespace DB {
  static constexpr int INVALID_FRAME = -1;

  /**
   * Frame
   *
   * Internal class to BufMgr, manages an individual slot in the buffer.
   */
  struct Frame {
    Frame();
    ~Frame();

    void pin();
    void unpin();
    bool isPinned() const;

    void    setPage(page_id pid, bool isEmpty = false);
    page_id getPageID() const;
    char *  getPage();

    void mark();
    void clean();
    bool isDirty() const;

    void evict();
    void free();
    bool isEmpty() const;

  private:

    page_id mPID;
    int     mPinCount;
    bool    mDirty;

    char    mData[Dim::PAGE_SIZE];
  };
}

#endif // DB_FRAME_H
