#include "heap_file.h"

#include <cstring>

#include "db.h"

namespace DB {
  HeapFile::HeapFile()
    : mLastPage ( nullptr )
    , mFirstPID ( newPage(mLastPage) )
    , mLastPID  ( mFirstPID )
  {}

  HeapFile::~HeapFile()
  {
    clear();
    Global::BUFMGR->unpin(mLastPID);
    Global::BUFMGR->bfree(mLastPID);
  }

  void
  HeapFile::append(int *data, int len)
  {
    // Check if there is enough space, and allocate a new page if there is not.
    if (mLastPage->count + len > PAGE_CAP) {
      HeapPage *newHP;
      page_id newPID  = newPage(newHP);
      mLastPage->next = newPID;
      mLastPage       = newHP;

      Global::BUFMGR->unpin(mLastPID, true);
      mLastPID = newPID;
    }

    memmove(&mLastPage->data[mLastPage->count], data,
            len * sizeof(int));

    mLastPage->count += len;
  }

  void
  HeapFile::clear()
  {
    // Traverse the chain, freeing pages as we go.
    while (mFirstPID != mLastPID) {
      HeapPage *hp = (HeapPage *)Global::BUFMGR->pin(mFirstPID);
      page_id nid = hp->next;

      Global::BUFMGR->unpin(mFirstPID);
      Global::BUFMGR->bfree(mFirstPID);
      mFirstPID = nid;
    }

    // Leave the last page around, but set its count to 0, as it is now our new
    // first page.
    mLastPage->count = 0;
  }

  page_id
  HeapFile::newPage(HeapFile::HeapPage *&hp)
  {
    char *buf;
    page_id newPID = Global::BUFMGR->bnew(buf);
    hp = (HeapPage *)buf;
    hp->count = 0;
    hp->next  = INVALID_PAGE;

    return newPID;
  }
}
