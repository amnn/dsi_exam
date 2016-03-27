#include "btrie.h"

#include "allocator.h"
#include "dim.h"

namespace DB {

  const int BTrie::BRANCH_STRIDE = 2;
  const int BTrie::BRANCH_SPACE  = (Dim::PAGE_SIZE - offsetof(BTrie, b.data)) / sizeof(int);
  const int BTrie::LEAF_SPACE    = (Dim::PAGE_SIZE - offsetof(BTrie, l.data)) / sizeof(int);

  page_id
  BTrie::leaf(int stride)
  {
    char *page;
    page_id lid = Global::BUFMGR->bnew(page);
    BTrie *leaf = (BTrie *)page;

    leaf->type     = Leaf;
    leaf->count    = 0;
    leaf->l.stride = stride;
    leaf->l.prev   = INVALID_PAGE;
    leaf->l.next   = INVALID_PAGE;

    Global::BUFMGR->unpin(lid, true);

    return lid;
  }

  page_id
  BTrie::branch(page_id left, int key, page_id right)
  {
    char *page;
    page_id bid = Global::BUFMGR->bnew(page);
    BTrie *branch = (BTrie *)page;

    branch->type = Branch;
    branch->count = 1;

    branch->slot(0)[-1] = left;
    branch->slot(0)[ 0] = key;
    branch->slot(0)[+1] = right;

    Global::BUFMGR->unpin(bid, true);

    return bid;
  }

  BTrie *
  BTrie::load(page_id nid)
  {
    return (BTrie *)Global::BUFMGR->pin(nid);
  }

  BTrie::SplitInfo
  BTrie::lookup(page_id nid, int key, page_id &pid, int &keyPos)
  {
    BTrie    *node  = load(nid);
    int       pos   = node->findKey(key);
    SplitInfo split = NO_CHANGE;

    switch (node->type) {
    case Leaf:
      pid = nid;

      // Add the key if it is not there.
      if (pos == node->count || *node->slot(pos) != key) {
        split = NO_SPLIT;

        // Split the node if it is full.
        if (node->isFull()) {
          int pivot;
          split = node->split(nid, pivot);

          if (pos >= pivot) {
            pid  = split.pid;
            pos -= pivot;

            Global::BUFMGR->unpin(nid, true);
            node = load(pid);
          }
        }

        // Make room for slot
        node->makeRoom(pos);
        *node->slot(pos) = key;
        Global::BUFMGR->unpin(pid, true);
      } else {
        Global::BUFMGR->unpin(pid);
      }

      keyPos = pos;
      break;
    case Branch: {
      int childPID = node->slot(pos)[-1];
      Global::BUFMGR->unpin(nid);

      // Traverse the appropriate child.
      auto childSplit = lookup(childPID, key, pid, keyPos);

      // If the child made no splits, return.
      if (!childSplit.isSplit()) {
        split = childSplit;
        break;
      } else {
        split = NO_SPLIT;
      }

      // Otherwise, put the new partition into this node
      // (Splitting if necessary.)
      node = load(nid);
      if (node->isFull()) {
        int pivot;
        split = node->split(nid, pivot);

        if (pos > pivot) {
          pos -= pivot + 1;
          Global::BUFMGR->unpin(nid, true);
          nid  = split.pid;
          node = load(nid);
        }
      }

      node->makeRoom(pos);
      *node->slot(pos) = childSplit.key;
      node->slot(pos)[1] = childSplit.pid;
      Global::BUFMGR->unpin(nid, true);

      break;
    }
    }

    return split;
  }

  int
  BTrie::findKey(int key)
  {
    int lo = 0, hi = count;

    while(lo < hi) {
      int m = lo + (hi - lo) / 2;
      int k = slot(m)[0];

      if      (key <= k) hi = m;
      else if (key >  k) lo = m + 1;
    }

    return lo;
  }

  BTrie::SplitInfo
  BTrie::split(page_id pid, int &pivot)
  {
    // Allocate a new page
    char *page;
    page_id nid = Global::BUFMGR->bnew(page);
    BTrie *node = (BTrie *)page;
    node->type = type;

    pivot = count / 2;
    int key;
    switch (type) {
    case Leaf:
      // Move half the records.
      memmove(node->slot(0), slot(pivot),
              (count - pivot) * l.stride * sizeof(int));

      node->count    = count - pivot;
      count          = pivot;
      node->l.stride = l.stride;

      // Fix the neighbour pointers.
      node->l.prev   = pid;
      node->l.next   = l.next;
      l.next         = nid;

      key = l.data[l.stride * (pivot - 1)];

      break;
    case Branch:
      // Move half the children, excluding the pivot key, which we push up.
      memmove(node->slot(0), slot(pivot) + 1,
              ((count - pivot) * BRANCH_STRIDE - 1) * sizeof(int));

      node->count = count - pivot - 1;
      count       = pivot;

      key = b.data[1 + BRANCH_STRIDE * pivot];
      break;
    }

    Global::BUFMGR->unpin(nid, true);
    return {.key = key, .pid = nid};
  }

  void
  BTrie::makeRoom(int index)
  {
    int stride = type == Leaf
      ? l.stride
      : BRANCH_STRIDE;

    memmove(slot(index + 1), slot(index),
            (count - index) * stride * sizeof(int));

    count++;
  }

  bool
  BTrie::isFull() const
  {
    switch (type) {
    case Leaf:
      return count >= LEAF_SPACE / l.stride;
    case Branch:
      return count >= (BRANCH_SPACE - 1) / BRANCH_STRIDE;
    default:
      throw std::runtime_error("Unrecognised Node Type");
    }
  }

  int *
  BTrie::slot(int index)
  {
    switch (type) {
    case Leaf:
      return &l.data[l.stride * index];
    case Branch:
      return &b.data[1 + BRANCH_STRIDE * index];
    default:
      throw std::runtime_error("Unrecognised Node Type");
    }
  }
}
