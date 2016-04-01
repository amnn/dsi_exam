#include "singleton_iterator.h"

#include <limits>
#include <stdexcept>

namespace DB {
  SingletonIterator::SingletonIterator(int fst, int x, int snd, int y)
    : mFst      ( fst )
    , mX        ( x )
    , mSnd      ( snd )
    , mY        ( y )
    , mDepth    ( -1 )
    , mFstAtEnd ( false )
    , mSndAtEnd ( false )
  {}

  void
  SingletonIterator::open()
  {
    if (atEnd())
      throw std::runtime_error("open: iterator finished!");

    mDepth++;
  }

  void SingletonIterator::up()   {
    mDepth--;

    mFstAtEnd &= (mDepth >= mFst);
    mSndAtEnd &= (mDepth >= mSnd);
  }

  void
  SingletonIterator::next()
  {
    mFstAtEnd |= (mDepth == mFst);
    mSndAtEnd |= (mDepth == mSnd);
  }

  void
  SingletonIterator::seek(int searchKey)
  {
    mFstAtEnd |= mDepth == mFst && searchKey > mX;
    mSndAtEnd |= mDepth == mSnd && searchKey > mY;
  }

  int
  SingletonIterator::key() const
  {
    if (atEnd())
      return std::numeric_limits<int>::max();

    if (mDepth == mFst)
      return mX;
    if (mDepth == mSnd)
      return mY;

    return std::numeric_limits<int>::min();
  }

  bool
  SingletonIterator::atEnd() const
  {
    return
      (mDepth == mFst && mFstAtEnd) ||
      (mDepth == mSnd && mSndAtEnd);
  }

  bool
  SingletonIterator::atValidDepth() const
  {
    return
      mDepth == mFst ||
      mDepth == mSnd;
  }
}
