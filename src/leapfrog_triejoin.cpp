#include "leapfrog_triejoin.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace DB {
  LeapFrogTrieJoin::LeapFrogTrieJoin(int joinSize,
                                     std::vector<std::unique_ptr<TrieIterator>> &&iters)
    : mJoinSize     ( joinSize )
    , mDepth        ( -1 )
    , mNextIter     ( 0 )
    , mKey          ( std::numeric_limits<int>::min() )
    , mAtEnd        ( false )
    , mActiveIters  {}
    , mDormantIters (std::move(iters))
  {}

  void
  LeapFrogTrieJoin::open()
  {
    mDepth++;

    if (!atValidDepth()) return;

    if (atEnd()) throw std::runtime_error("open: Iterator finished!");

    for (auto &it : mActiveIters)
      it->open();

    for (auto &it : mDormantIters)
      it->open();

    init();
  }

  void
  LeapFrogTrieJoin::up()
  {
    mDepth--;

    if(!atValidDepth()) return;

    for (auto &it : mActiveIters)
      it->up();

    for (auto &it : mDormantIters)
      it->up();

    init();
  }

  void
  LeapFrogTrieJoin::next()
  {
    if (!atValidDepth() || atEnd()) return;

    auto &iter = mActiveIters[mNextIter];
    iter->next();

    if (iter->atEnd()) {
      mAtEnd = true;
    } else {
      mNextIter = (mNextIter + 1) % mActiveIters.size();
      search();
    }
  }

  void
  LeapFrogTrieJoin::seek(int searchKey)
  {
    if (!atValidDepth() || atEnd()) return;

    auto &iter = mActiveIters[mNextIter];
    iter->seek(searchKey);

    if (iter->atEnd()) {
      mAtEnd = true;
    } else {
      mNextIter = (mNextIter + 1) % mActiveIters.size();
      search();
    }
  }

  int
  LeapFrogTrieJoin::key() const
  {
    if (!atValidDepth())
      return std::numeric_limits<int>::min();

    return mKey;
  }

  bool
  LeapFrogTrieJoin::atEnd() const
  {
    return atValidDepth() && mAtEnd;
  }

  bool
  LeapFrogTrieJoin::atValidDepth() const
  {
    return 0 <= mDepth && mDepth < mJoinSize;
  }

  void
  LeapFrogTrieJoin::init()
  {
    std::vector<std::unique_ptr<TrieIterator>>
      newActive {}, newDormant {};

    mAtEnd = false;
    // Re-partition iterators into active and dormant.
    for (auto &it : mActiveIters) {
      mAtEnd |= it->atEnd();

      if (it->atValidDepth())
        newActive.emplace_back(std::move(it));
      else
        newDormant.emplace_back(std::move(it));
    }

    for (auto &it : mDormantIters) {
      mAtEnd |= it->atEnd();

      if (it->atValidDepth())
        newActive.emplace_back(std::move(it));
      else
        newDormant.emplace_back(std::move(it));
    }

    mActiveIters  = std::move(newActive);
    mDormantIters = std::move(newDormant);

    std::sort(mActiveIters.begin(),
              mActiveIters.end(),
              [](const auto &it1, const auto &it2) {
                return it1->key() < it2->key();
              });

    mNextIter = 0;

    search();
  }

  void
  LeapFrogTrieJoin::search()
  {
    int num    = mActiveIters.size();
    int prev   = (mNextIter - 1 + num) % num;
    int maxKey = mActiveIters[prev]->key();

    while(true) {
      auto &iter  = mActiveIters[mNextIter];
      int nextKey = iter->key();
      if (nextKey == maxKey) {
        mKey = nextKey;
        return;
      } else {
        iter->seek(maxKey);
        if (iter->atEnd()) {
          mAtEnd = true;
          return;
        } else {
          maxKey = iter->key();
          mNextIter = (mNextIter + 1) % num;
        }
      }
    }
  }
}
