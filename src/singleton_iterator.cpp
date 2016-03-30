#include "singleton_iterator.h"

#include <limits>
#include <stdexcept>

namespace DB {
  SingletonIterator::SingletonIterator(int fst, int x, int snd, int y)
    : mFst   ( fst )
    , mX     ( x )
    , mSnd   ( snd )
    , mY     ( y )
    , mDepth ( -1 )
    , mAtEnd ( false )
  {}

  void
  SingletonIterator::open()
  {
    if (mAtEnd)
      throw std::runtime_error("open: iterator finished!");

    mDepth++;

    if (atValidDepth()) mAtEnd = false;
  }

  void SingletonIterator::up()   { mDepth--; }

  void
  SingletonIterator::next()
  {
    if (atValidDepth()) mAtEnd = true;
  }

  void
  SingletonIterator::seek(int searchKey)
  {
    mAtEnd |=
      (mDepth == mFst && searchKey > mX) ||
      (mDepth == mSnd && searchKey > mY);
  }

  int
  SingletonIterator::key() const
  {
    if (mAtEnd)
      return std::numeric_limits<int>::max();

    if (mDepth == mFst)
      return mX;
    if (mDepth == mSnd)
      return mY;

    return std::numeric_limits<int>::min();
  }

  bool SingletonIterator::atEnd() const { return mAtEnd; }

  bool
  SingletonIterator::atValidDepth() const
  {
    return
      mDepth == mFst ||
      mDepth == mSnd;
  }
}
