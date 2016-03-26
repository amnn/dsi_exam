#include "frame.h"

#include <cstring>

#include "allocator.h"
#include "db.h"
#include "dim.h"

namespace DB {
  Frame::Frame()
    : mPID(INVALID_PAGE)
    , mPinCount(0)
    , mDirty(false)
  {}

  Frame::~Frame()              { evict(); }

  void Frame::pin()            { mPinCount++; }
  void Frame::unpin()          { mPinCount--; }
  bool Frame::isPinned() const { return mPinCount > 0; }

  void
  Frame::setPage(page_id pid, bool isEmpty)
  {
    mPID = pid;
    if(isEmpty)
      memset(mData, 0, Dim::PAGE_SIZE);
    else
      Global::ALLOC->read(mPID, &mData[0]);
  }

  page_id Frame::getPageID() const { return mPID; }
  char *  Frame::getPage()         { return &mData[0]; }

  void Frame::mark()          { mDirty |= true; }
  void Frame::clean()         { mDirty = false; }
  bool Frame::isDirty() const { return mDirty; }

  void
  Frame::evict()
  {
    if (mDirty && mPID != INVALID_PAGE)
      Global::ALLOC->write(mPID, &mData[0]);

    free();
  }

  void
  Frame::free()
  {
    mPID      = INVALID_PAGE;
    mPinCount = 0;
    mDirty    = false;
  }

  bool Frame::isEmpty() const { return mPID == INVALID_PAGE; }
}
