#include "btrie_iterator.h"

#include <limits>
#include <stdexcept>

#include "allocator.h"
#include "db.h"

namespace DB {
  BTrieIterator::BTrieIterator(page_id rootPID, int fst, int snd)
    : mFst       ( fst )
    , mSnd       ( snd )
    , mDummy     ( (BTrie *) BTrie::onHeap(2, 1) )
    , mHistory   {}
    , mCurrDepth ( -1 )
    , mNodeDepth ( -1 )
    , mPID       ( INVALID_PAGE )
    , mCurr      ( mDummy )
    , mPos       ( 0 )
  {
    mDummy->slot(0)[1] = rootPID;
  }

  BTrieIterator::~BTrieIterator()
  {
    if (mPID != INVALID_PAGE)
      Global::BUFMGR->unpin(mPID);

    delete[] (char *)mDummy;
  }

  void
  BTrieIterator::open()
  {
    if (atEnd()) throw std::runtime_error("open: iterator finished!");

    mCurrDepth++;
    if (!(atValidDepth()    ||
          mCurrDepth == -1) ||
        mCurrDepth == mNodeDepth)
      return;

    // Save position at current level
    mHistory.emplace(std::make_tuple(mPID, mPos, mNodeDepth));

    // Find the leftmost child
    int cid = mCurr->slot(mPos)[1];
    if (mPID != INVALID_PAGE)
      Global::BUFMGR->unpin(mPID);

    mPos  = 0;
    mPID  = cid;
    mCurr = BTrie::load(mPID);
    while (mCurr->getType() != Leaf) {
      cid   = mCurr->slot(0)[-1];
      Global::BUFMGR->unpin(mPID);
      mPID  = cid;
      mCurr = BTrie::load(mPID);
    }

    mNodeDepth = mCurrDepth;
  }

  void
  BTrieIterator::up()
  {
    mCurrDepth--;
    if (mCurrDepth >= mNodeDepth)
      return;

    // Recover old position from history and swap it in.
    if (mPID != INVALID_PAGE)
      Global::BUFMGR->unpin(mPID);

    auto past  = mHistory.top();
    mPID       = std::get<0>(past);
    mPos       = std::get<1>(past);
    mNodeDepth = std::get<2>(past);

    mCurr = mPID == INVALID_PAGE
      ? mDummy
      : BTrie::load(mPID);

    mHistory.pop();
  }

  void
  BTrieIterator::next()
  {
    if (!atValidDepth() || atEnd()) return;

    mPos++;

    if (mPos < mCurr->getCount()) return;

    // Find next non-empty page (or the end).
    page_id nid = mCurr->getNext();
    if (nid != INVALID_PAGE) {
      Global::BUFMGR->unpin(mPID);
      mPos  = 0;
      mPID  = nid;
      mCurr = BTrie::load(mPID);
    }
  }

  void
  BTrieIterator::seek(int searchKey)
  {
    if (!atValidDepth() || atEnd()) return;

    searchKey   = std::max(searchKey, key());

    auto past   = mHistory.top();
    page_id pid = std::get<0>(past);
    int     pos = std::get<1>(past);

    page_id rootPID;
    if (pid == INVALID_PAGE) {
      rootPID = mDummy->slot(pos)[1];
    } else {
      rootPID = BTrie::load(pid)->slot(pos)[1];
      Global::BUFMGR->unpin(pid);
    }

    Global::BUFMGR->unpin(mPID);
    BTrie::find(rootPID, searchKey, mPID, mPos);
    mCurr = BTrie::load(mPID);
  }

  int
  BTrieIterator::key() const
  {
    if (!atValidDepth())
      return std::numeric_limits<int>::min();

    if (atEnd())
      return std::numeric_limits<int>::max();

    return mCurr->slot(mPos)[0];
  }

  bool
  BTrieIterator::atEnd() const
  {
    return
      atValidDepth()            &&
      mPos >= mCurr->getCount() &&
      mCurr->getNext() == INVALID_PAGE;
  }

  bool
  BTrieIterator::atValidDepth() const
  {
    return
      mCurrDepth == mFst ||
      mCurrDepth == mSnd;
  }
}
