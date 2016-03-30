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
    mCurrDepth++;
    if (!atValidDepth() || mCurrDepth == mNodeDepth)
      return;

    // Save position at current level
    mHistory.emplace(std::make_pair(mPID, mPos));

    // Find the leftmost child
    int cid = mCurr->slot(mPos)[1];
    if (mPID != INVALID_PAGE)
      Global::BUFMGR->unpin(mPID);

    mPos  = 0;
    mPID  = cid;
    mCurr = BTrie::load(mPID);
    while (mCurr->getType() != BTrie::Leaf) {
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
    if (!atValidDepth() || mCurrDepth == mNodeDepth)
      return;

    // Recover old position from history and swap it in.
    if (mPID != INVALID_PAGE)
      Global::BUFMGR->unpin(mPID);

    mNodeDepth = mCurrDepth;

    auto past = mHistory.top();
    mPID = std::get<0>(past);
    mPos = std::get<1>(past);

    mCurr = mPID == INVALID_PAGE
      ? mDummy
      : BTrie::load(mPID);

    mHistory.pop();
  }

  void
  BTrieIterator::next()
  {
    if (!atValidDepth() || mCurrDepth < 0 || atEnd()) return;

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
    if (!atValidDepth() || mCurrDepth < 0 || atEnd()) return;

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
    if (!atValidDepth() || mCurrDepth < 0)
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
      mCurrDepth >= 0           &&
      mPos >= mCurr->getCount() &&
      mCurr->getNext() == INVALID_PAGE;
  }

  bool
  BTrieIterator::atValidDepth() const
  {
    return
      mCurrDepth == -1   ||
      mCurrDepth == mFst ||
      mCurrDepth == mSnd;
  }
}
