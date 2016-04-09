#include "bufmgr.h"

#include <stdexcept>

#include "allocator.h"
#include "db.h"
#include "frame.h"

namespace DB {
  BufMgr::BufMgr(int poolSize)
    : mFrames(new Frame[poolSize])
    , mReplacer(mFrames, poolSize)
    , mPoolSize(poolSize)
  {}

  BufMgr::~BufMgr() { delete[] mFrames; }

  char *
  BufMgr::pin(page_id pid, bool isEmpty)
  {
    if (pid == INVALID_PAGE)
      return nullptr;

    int fid = findFrame(pid);
    if (fid == INVALID_FRAME) {
      fid = mReplacer.pickVictim();
      if (fid == INVALID_FRAME)
        throw std::runtime_error("No Free Frames!");
    }

    Frame &frame = mFrames[fid];
    if (frame.getPageID() != pid) {
      frame.evict();
      frame.setPage(pid, isEmpty);
    }

    frame.pin();
    mReplacer.framePinned(fid);
    return frame.getPage();
  }

  void
  BufMgr::unpin(page_id pid, bool dirty)
  {
    if (pid == INVALID_PAGE)
      throw std::runtime_error("Invalid Page!");

    int fid = findFrame(pid);
    if (fid == INVALID_FRAME)
      throw std::runtime_error("Page Not Pinned!");

    Frame &frame = mFrames[fid];
    if (frame.getPageID() != pid || !frame.isPinned())
      throw std::runtime_error("Page Not Pinned!");

    if (dirty) frame.mark();
    frame.unpin();
    mReplacer.frameUnpinned(fid);
  }

  page_id
  BufMgr::bnew(char *&first, int howMany)
  {
    page_id pid0 = Global::ALLOC->palloc(howMany);

    first = pin(pid0, true);
    if (first == nullptr) {
      Global::ALLOC->pfree(pid0, howMany);
      return INVALID_PAGE;
    }

    return pid0;
  }

  void
  BufMgr::bfree(page_id pid)
  {
    if (pid == INVALID_PAGE) return;

    int fid = findFrame(pid);
    if (fid == INVALID_FRAME) {
      Global::ALLOC->pfree(pid);
      return;
    }

    Frame &frame = mFrames[fid];
    if (frame.getPageID() != pid) {
      Global::ALLOC->pfree(pid);
      return;
    }

    frame.unpin();
    if (frame.isPinned()) {
      frame.pin();
      throw std::runtime_error("Attempted to free pinned page!");
    }

    frame.free();
    Global::ALLOC->pfree(pid);
  }

  void
  BufMgr::flush(page_id pid)
  {
    if (pid == INVALID_PAGE)
      throw std::runtime_error("Flushing invalid page");

    int fid = findFrame(pid);
    if (fid == INVALID_FRAME)
      throw std::runtime_error("Flushing uncached page");

    Frame &frame = mFrames[fid];
    if (frame.getPageID() != pid)
      throw std::runtime_error("Flushing uncached page");

    if (frame.isPinned())
      throw std::runtime_error("Flushing pinned page");

    frame.evict();
  }

  int
  BufMgr::findFrame(page_id pid)
  {
    int lastEmpty = INVALID_FRAME;

    for (int i = 0; i < mPoolSize; ++i) {
      Frame &frame = mFrames[i];
      if (frame.isEmpty())
        lastEmpty = i;
      else if (frame.getPageID() == pid)
        return i;
    }

    return lastEmpty;
  }
}
