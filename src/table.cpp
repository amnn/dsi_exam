#include "table.h"

#include <cstring>
#include <utility>
#include <stdexcept>

#include "allocator.h"
#include "btrie.h"
#include "bufmgr.h"
#include "db.h"

namespace DB {

  Table::Table(int order1, int order2)
    : mRootPID    { BTrie::leaf(2) }
    , mRootOrder  { std::min(order1, order2) }
    , mSubOrder   { std::max(order1, order2) }
    , mIsReversed { order1 > order2 }
  {}

  bool
  Table::insert(int x, int y)
  {
    if (mIsReversed)
      std::swap(x, y);

    page_id rootLID; int rootPos;
    auto rootSplit = BTrie::reserve(mRootPID, x, BTrie::NO_SIBS, rootLID, rootPos);

    // If no insertion was needed.
    if (rootSplit.isNoChange()) {
      BTrie * rootLeaf = BTrie::load(rootLID);
      page_id subPID   = rootLeaf->slot(rootPos)[1];

      page_id subLID; int subPos;
      auto subSplit = BTrie::reserve(subPID, y, BTrie::NO_SIBS, subLID, subPos);

      // A split occurred in the sub index, so we need to create a new root node
      // for it and replace the slot in the leaf of the root index
      if (subSplit.isSplit()) {
        rootLeaf->slot(rootPos)[1] = BTrie::branch(subPID, subSplit.key, subSplit.pid);
        Global::BUFMGR->unpin(rootLID, true);
      } else {
        Global::BUFMGR->unpin(rootLID);
      }

      return !subSplit.isNoChange();
    }

    // Otherwise the reservation caused an insertion.

    // Update the root PID if we had to split it.
    if (rootSplit.isSplit()) {
      mRootPID = BTrie::branch(mRootPID, rootSplit.key, rootSplit.pid);
    }

    // We must create a new sub index to fill this slot, and put the `y` in there.
    page_id newLID = BTrie::leaf(1);

    page_id subLID; int subPos;
    BTrie::reserve(newLID, y, BTrie::NO_SIBS, subLID, subPos);

    // Then we update the leaf with the page_id of the new sub index.
    BTrie * rootLeaf = BTrie::load(rootLID);
    rootLeaf->slot(rootPos)[1] = newLID;
    Global::BUFMGR->unpin(rootLID, true);

    return true;
  }
}
