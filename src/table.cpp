#include "table.h"

#include <cstring>
#include <utility>
#include <stdexcept>

#include "allocator.h"
#include "btrie.h"
#include "btrie_iterator.h"
#include "bufmgr.h"
#include "db.h"
#include "singleton_iterator.h"

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
    if (rootSplit.prop == BTrie::PROP_NOTHING) {
      BTrie * rootLeaf = BTrie::load(rootLID);
      page_id subPID   = rootLeaf->slot(rootPos)[1];

      page_id subLID; int subPos;
      auto subSplit = BTrie::reserve(subPID, y, BTrie::NO_SIBS, subLID, subPos);

      // A split occurred in the sub index, so we need to create a new root node
      // for it and replace the slot in the leaf of the root index
      if (subSplit.prop == BTrie::PROP_SPLIT) {
        rootLeaf->slot(rootPos)[1] = BTrie::branch(subPID, subSplit.key, subSplit.pid);
        Global::BUFMGR->unpin(rootLID, true);
      } else {
        Global::BUFMGR->unpin(rootLID);
      }

      return subSplit.prop != BTrie::PROP_NOTHING;
    }

    // Otherwise the reservation caused an insertion.

    // Update the root PID if we had to split it.
    if (rootSplit.prop == BTrie::PROP_SPLIT) {
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

  bool
  Table::remove(int x, int y)
  {
    if (mIsReversed)
      std::swap(x, y);

    bool didChange = false;
    BTrie::deleteIf(mRootPID, x, { .sibs = BTrie::NO_SIBS },
                    [&didChange, y] (page_id rootLID, int rootPos) {
                      BTrie *rootLeaf = BTrie::load(rootLID);
                      page_id subPID  = rootLeaf->slot(rootPos)[1];

                      // Delete the key in the sub-index.
                      BTrie::deleteIf(subPID, y, { .sibs = BTrie::NO_SIBS },
                                      [&didChange] (page_id, int) {
                                        didChange = true;
                                        return true;
                                      });

                      // Deal with the Sub-Index having an empty root.
                      BTrie *sub = BTrie::load(subPID);

                      if (sub->isEmpty()) {
                        switch (sub->getType()) {
                        case BTrie::Leaf:
                          // Delete this sub-index entirely.
                          Global::BUFMGR->unpin(subPID);
                          Global::BUFMGR->bfree(subPID);

                          Global::BUFMGR->unpin(rootLID);
                          return true;
                        case BTrie::Branch:
                          // Replace the branch with its only child.
                          rootLeaf->slot(rootPos)[1] = sub->slot(0)[-1];

                          Global::BUFMGR->unpin(subPID);
                          Global::BUFMGR->bfree(subPID);

                          Global::BUFMGR->unpin(rootLID, true);
                          return false;
                        }
                      }

                      Global::BUFMGR->unpin(subPID);
                      Global::BUFMGR->unpin(rootLID);
                      return false;
                    });

    // Deal with the Root Index having an empty root node.
    BTrie *root = BTrie::load(mRootPID);
    if (root->isEmpty() && root->getType() == BTrie::Branch) {
      // Replace the branch with its only child.
      page_id newRoot = root->slot(0)[-1];
      Global::BUFMGR->unpin(mRootPID);
      Global::BUFMGR->bfree(mRootPID);
      mRootPID = newRoot;
    } else {
      Global::BUFMGR->unpin(mRootPID);
    }

    return didChange;
  }

  std::unique_ptr<TrieIterator>
  Table::scan()
  {
    BTrieIterator *it = new BTrieIterator(mRootPID, mRootOrder, mSubOrder);
    return std::unique_ptr<TrieIterator>(it);
  }

  std::unique_ptr<TrieIterator>
  Table::singleton(int x, int y)
  {
    if (mIsReversed) std::swap(x, y);

    SingletonIterator *it = new SingletonIterator(mRootOrder, x, mSubOrder, y);
    return std::unique_ptr<TrieIterator>(it);
  }
}
