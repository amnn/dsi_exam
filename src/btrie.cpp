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
    leaf->prev     = INVALID_PAGE;
    leaf->next     = INVALID_PAGE;
    leaf->l.stride = stride;

    Global::BUFMGR->unpin(lid, true);

    return lid;
  }

  page_id
  BTrie::branch(page_id left, int key, page_id right)
  {
    char *page;
    page_id bid = Global::BUFMGR->bnew(page);
    BTrie *branch = (BTrie *)page;

    branch->type  = Branch;
    branch->count = 1;
    branch->prev  = INVALID_PAGE;
    branch->next  = INVALID_PAGE;

    branch->slot(0)[-1] = left;
    branch->slot(0)[ 0] = key;
    branch->slot(0)[+1] = right;

    Global::BUFMGR->unpin(bid, true);

    return bid;
  }

  char *
  BTrie::onHeap(int stride, int size)
  {
    std::size_t bytes = offsetof(BTrie, l.data);
    bytes            += size * stride * sizeof(int);

    char  * buf  = new char[bytes]();
    BTrie * node = (BTrie *)buf;

    node->type     = Leaf;
    node->count    = size;
    node->prev     = INVALID_PAGE;
    node->next     = INVALID_PAGE;
    node->l.stride = stride;

    return buf;
  }

  BTrie *
  BTrie::load(page_id nid)
  {
    return (BTrie *)Global::BUFMGR->pin(nid);
  }

  BTrie::Diff
  BTrie::reserve(page_id nid, int key, Siblings sibs, page_id &pid, int &keyPos)
  {
    BTrie * node  = load(nid);
    int     pos   = node->findKey(key);
    Diff    split = {};
    split.prop = PROP_NOTHING;

    switch (node->type) {
    case Leaf:
      pid = nid;

      // Add the key if it is not there.
      if (pos == node->count || *node->slot(pos) != key) {
        split.prop = PROP_CHANGE;

        // Try Redistributing Left
        if (node->isFull() && (LEFT_SIB & sibs)) {
          BTrie *left = load(node->prev);

          if (left->isFull()) {
            Global::BUFMGR->unpin(node->prev);
          } else {
            split.prop = PROP_REDISTRIB;
            split.sib  = LEFT_SIB;

            int total = node->count + left->count + 1;
            int delta = total / 2 - left->count;

            // Fix indices according to which node the new key belongs to.
            if (pos < delta) {
              delta--;
              pid  = node->prev;
              pos += left->count;
            } else {
              pos -= delta;
            }

            // Shift the nodes over
            memmove(left->slot(left->count), node->slot(0),
                    delta * node->l.stride * sizeof(int));
            left->count += delta;
            node->makeRoom(delta, -delta);

            split.key = pid == node->prev && pos == left->count
              ? key
              : left->slot(left->count - 1)[0];

            // Clean up
            if (pid == nid) {
              Global::BUFMGR->unpin(node->prev, true);
            } else {
              Global::BUFMGR->unpin(nid, true);
              node = left;
            }
          }
        }

        // Try Redistributing Right
        if (node->isFull() && (RIGHT_SIB & sibs)) {
          BTrie *right = load(node->next);

          if (right->isFull()) {
            Global::BUFMGR->unpin(node->next);
          } else {
            split.prop = PROP_REDISTRIB;
            split.sib  = RIGHT_SIB;

            int total = node->count + right->count + 1;
            int delta = total / 2 - right->count;
            int keep  = node->count - delta;

            if (pos > keep) {
              delta--;
              pos -= keep + 1;
              pid  = node->next;
            }

            right->makeRoom(0, delta);
            memmove(right->slot(0), node->slot(keep),
                    delta * node->l.stride * sizeof(int));
            node->count = keep;

            split.key = pid == nid && pos == keep
              ? key
              : node->slot(node->count - 1)[0];

            if (pid == nid) {
              Global::BUFMGR->unpin(node->next, true);
            } else {
              Global::BUFMGR->unpin(nid, true);
              node = right;
            }
          }
        }

        // We have no choice but to split.
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

      Siblings childSibs = NO_SIBS;
      if (pos > 0)            childSibs |= LEFT_SIB;
      if (pos < node->count ) childSibs |= RIGHT_SIB;

      Global::BUFMGR->unpin(nid);

      // Traverse the appropriate child.
      auto childSplit = reserve(childPID, key, childSibs, pid, keyPos);

      // If we don't need to update this node, then return.
      if (childSplit.prop != PROP_SPLIT && childSplit.prop != PROP_REDISTRIB) {
        split = childSplit;
        break;
      } else {
        split.prop = PROP_CHANGE;
      }

      node = load(nid);
      // The child redistributed, we just need to update the partitioning key.
      if (childSplit.prop == PROP_REDISTRIB) {
        if (childSplit.sib == RIGHT_SIB)
          node->slot(pos)[0] = childSplit.key;
        else
          node->slot(pos - 1)[0] = childSplit.key;

        Global::BUFMGR->unpin(nid, true);
        break;
      }

      // The child was split, we'll need to insert a new slot. (We might need to
      // split ourselves).
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
      node->slot(pos)[0] = childSplit.key;
      node->slot(pos)[1] = childSplit.pid;
      Global::BUFMGR->unpin(nid, true);

      break;
    }
    }

    return split;
  }

  BTrie::Diff
  BTrie::deleteIf(page_id nid, int key,
                  Family family,
                  std::function<bool(page_id, int)> predicate)
  {
    BTrie * node = load(nid);
    int     pos  = node->findKey(key);
    Diff    diff = {};
    diff.prop = PROP_NOTHING;

    switch (node->type) {
    case Leaf:
      if (pos == node->count
          || node->slot(pos)[0] != key
          || !predicate(nid, pos)
          ) {
        Global::BUFMGR->unpin(nid);
        break;
      }

      // Delete the slot
      diff.prop = PROP_CHANGE;
      node->makeRoom(pos + 1, -1);

      if (!node->isUnderOccupied()) {
        Global::BUFMGR->unpin(nid, true);
        break;
      }

      // Try Redistributing Left
      if (family.sibs & LEFT_SIB) {
        BTrie *left = load(node->prev);

        if (left->isUnderOccupied()) {
          Global::BUFMGR->unpin(node->prev);
        } else {
          diff.prop = PROP_REDISTRIB;
          diff.sib  = LEFT_SIB;

          int total = node->count + left->count;
          int delta = (total - 1) / 2 - node->count + 1;
          int keep  = left->count - delta;

          node->makeRoom(0, delta);
          memmove(node->slot(0), left->slot(keep),
                  delta * node->l.stride * sizeof(int));
          left->count = keep;

          diff.key = left->slot(left->count - 1)[0];

          Global::BUFMGR->unpin(node->prev, true);
          Global::BUFMGR->unpin(nid, true);
          break;
        }
      }

      // Try Redistributing Right
      if (family.sibs & RIGHT_SIB) {
        BTrie *right = load(node->next);

        if (right->isUnderOccupied()) {
          Global::BUFMGR->unpin(node->next);
        } else {
          diff.prop = PROP_REDISTRIB;
          diff.sib  = RIGHT_SIB;

          int total = node->count + right->count;
          int delta = (total - 1) / 2 - node->count + 1;

          memmove(node->slot(node->count), right->slot(0),
                  delta * node->l.stride * sizeof(int));
          node->count += delta;
          right->makeRoom(delta, -delta);

          diff.key = node->slot(node->count - 1)[0];

          Global::BUFMGR->unpin(node->next, true);
          Global::BUFMGR->unpin(nid, true);
          break;
        }
      }

      // Try Merging Left
      if (family.sibs & LEFT_SIB) {
        page_id lid = node->prev;
        BTrie *left = load(lid);
        diff.prop = PROP_MERGE;
        diff.sib  = LEFT_SIB;

        left->merge(lid, node, family.leftKey);

        Global::BUFMGR->unpin(lid, true);
        Global::BUFMGR->unpin(nid);
        break;
      }

      // Try Merging Right
      if (family.sibs & RIGHT_SIB) {
        page_id rid  = node->next;
        BTrie *right = load(rid);
        diff.prop = PROP_MERGE;
        diff.sib  = RIGHT_SIB;

        node->merge(nid, right, family.rightKey);

        Global::BUFMGR->unpin(nid, true);
        Global::BUFMGR->unpin(rid);
        break;
      }

      Global::BUFMGR->unpin(nid, true);
      break;
    case Branch: {
      int childPID = node->slot(pos)[-1];

      Family childFamily {};
      if (pos > 0) {
        childFamily.sibs    |= LEFT_SIB;
        childFamily.leftKey  = node->slot(pos - 1)[0];
      }

      if (pos < node->count) {
        childFamily.sibs |= RIGHT_SIB;
        childFamily.rightKey = node->slot(pos)[0];
      }

      Global::BUFMGR->unpin(nid);

      // Traverse the appropriate child.
      Diff childDiff = deleteIf(childPID, key, childFamily, predicate);

      // If we don't need to update this node, then return.
      if (childDiff.prop != PROP_MERGE && childDiff.prop != PROP_REDISTRIB) {
        diff = childDiff;
        break;
      } else {
        diff.prop = PROP_CHANGE;
      }

      // Fix the partitioning key in the case of a redistribution.
      node = load(nid);
      if (childDiff.prop == PROP_REDISTRIB) {
        if (childDiff.sib == RIGHT_SIB)
          node->slot(pos)[0] = childDiff.key;
        else
          node->slot(pos - 1)[0] = childDiff.key;

        Global::BUFMGR->unpin(nid, true);
        break;
      }

      // Otherwise we must deal with a merge.
      if (childDiff.sib == RIGHT_SIB) {
        int toFree = node->slot(pos)[+1];

        node->makeRoom(pos + 1, -1);
        Global::BUFMGR->bfree(toFree);
      } else if (childDiff.sib == LEFT_SIB) {
        int toFree = node->slot(pos)[-1];

        node->makeRoom(pos, -1);
        Global::BUFMGR->bfree(toFree);
      }

      if (!node->isUnderOccupied()) {
        Global::BUFMGR->unpin(nid, true);
        break;
      }

      // Try Redistributing Left
      if (family.sibs & LEFT_SIB) {
        BTrie *left = load(node->prev);

        if (left->isUnderOccupied()) {
          Global::BUFMGR->unpin(node->prev);
        } else {
          diff.prop = PROP_REDISTRIB;
          diff.sib  = LEFT_SIB;

          int total = node->count + left->count;
          int delta = (total - 1)/ 2 - node->count + 1;
          int keep  = left->count - delta;

          node->makeRoom(0, delta);
          node->slot(delta - 1)[0] = family.leftKey;
          node->slot(delta - 1)[1] = node->slot(0)[-1];
          memmove(node->slot(0) - 1, left->slot(keep) + 1,
                  (delta * BRANCH_STRIDE - 1) * sizeof(int));
          left->count = keep;

          diff.key = left->slot(left->count)[0];

          Global::BUFMGR->unpin(node->prev, true);
          Global::BUFMGR->unpin(nid, true);
          break;
        }
      }

      // Try Redistributing Right
      if (family.sibs & RIGHT_SIB) {
        BTrie *right = load(node->next);

        if (right->isUnderOccupied()) {
          Global::BUFMGR->unpin(node->next);
        } else {
          diff.prop = PROP_REDISTRIB;
          diff.sib  = RIGHT_SIB;

          int total = node->count + right->count;
          int delta = (total - 1) / 2 - node->count + 1;

          node->slot(node->count)[0] = family.rightKey;
          memmove(node->slot(node->count) + 1, right->slot(0) - 1,
                  (delta * BRANCH_STRIDE - 1) * sizeof(int));
          node->count += delta;

          diff.key           = right->slot(delta - 1)[0];
          right->slot(0)[-1] = right->slot(delta)[-1];

          right->makeRoom(delta, -delta);

          Global::BUFMGR->unpin(node->next, true);
          Global::BUFMGR->unpin(nid, true);
          break;
        }
      }

      // Try Merging Left
      if (family.sibs & LEFT_SIB) {
        page_id lid = node->prev;
        BTrie *left = load(lid);
        diff.prop   = PROP_MERGE;
        diff.sib    = LEFT_SIB;

        left->merge(lid, node, family.leftKey);

        Global::BUFMGR->unpin(lid, true);
        Global::BUFMGR->unpin(nid);
        break;
      }

      // Try Merging Right
      if (family.sibs & RIGHT_SIB) {
        page_id rid  = node->next;
        BTrie *right = load(rid);
        diff.prop    = PROP_MERGE;
        diff.sib     = RIGHT_SIB;

        node->merge(nid, right, family.rightKey);
        Global::BUFMGR->unpin(nid, true);
        Global::BUFMGR->unpin(rid);
        break;
      }

      Global::BUFMGR->unpin(nid, true);
      break;
    }
    }

    return diff;
  }

  void
  BTrie::find(page_id nid, int key, page_id &foundPID, int &foundPos)
  {
    BTrie * node = load(nid);
    int     pos  = node->findKey(key);

    switch (node->type) {
    case Leaf:
      if (pos >= node->count &&
          node->next != INVALID_PAGE) {
        foundPID = node->next;
        foundPos = 0;
      } else {
        foundPID = nid;
        foundPos = pos;
      }
      Global::BUFMGR->unpin(nid);
      break;
    case Branch: {
      int childPID = node->slot(pos)[-1];
      Global::BUFMGR->unpin(nid);
      find(childPID, key, foundPID, foundPos);
      break;
    }
    }
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

  BTrie::Diff
  BTrie::split(page_id pid, int &pivot)
  {
    // Allocate a new page
    char *page;
    page_id nid = Global::BUFMGR->bnew(page);
    BTrie *node = (BTrie *)page;
    node->type = type;

    Diff diff {};
    diff.prop = PROP_SPLIT;
    diff.pid  = nid;

    pivot = count / 2;
    switch (type) {
    case Leaf:
      // Move half the records.
      memmove(node->slot(0), slot(pivot),
              (count - pivot) * l.stride * sizeof(int));

      node->count    = count - pivot;
      count          = pivot;
      node->l.stride = l.stride;

      diff.key = slot(pivot - 1)[0];
      break;
    case Branch:
      // Move half the children, excluding the pivot key, which we push up.
      memmove(node->slot(0) - 1, slot(pivot + 1) - 1,
              ((count - pivot) * BRANCH_STRIDE - 1) * sizeof(int));

      node->count = count - pivot - 1;
      count       = pivot;

      diff.key = slot(pivot)[0];
      break;
    }

    // Fix the neighbour pointers.
    node->prev = pid;
    node->next = next;
    next       = nid;

    if (node->next != INVALID_PAGE) {
      BTrie *nbr = load(node->next);
      nbr->prev = nid;
      Global::BUFMGR->unpin(node->next, true);
    }

    Global::BUFMGR->unpin(nid, true);

    return diff;
  }

  void
  BTrie::merge(page_id nid, BTrie *that, int part)
  {
    switch (type) {
    case Leaf:
      memmove(slot(count), that->slot(0),
              that->count * l.stride * sizeof(int));
      count += that->count;
      break;
    case Branch:
      slot(count)[0] = part;
      memmove(slot(count) + 1, that->slot(0) - 1,
              (1 + that->count * BRANCH_STRIDE) * sizeof(int));
      count += that->count + 1;
      break;
    }

    // Fix neighbour pointers
    next = that->next;

    if (next != INVALID_PAGE) {
      BTrie *newNext = load(next);
      newNext->prev = nid;
      Global::BUFMGR->unpin(next, true);
    }
  }

  void
  BTrie::makeRoom(int index, int size)
  {
    int stride = type == Leaf
      ? l.stride
      : BRANCH_STRIDE;

    memmove(slot(index + size), slot(index),
            (count - index) * stride * sizeof(int));

    count += size;
  }

  BTrie::NodeType
  BTrie::getType() const
  {
    return type;
  }

  int
  BTrie::getCount() const
  {
    return count;
  }

  page_id
  BTrie::getPrev() const
  {
    return prev;
  }

  page_id
  BTrie::getNext() const
  {
    return next;
  }

  bool
  BTrie::isEmpty() const
  {
    return count == 0;
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

  bool
  BTrie::isUnderOccupied() const
  {
    switch (type) {
    case Leaf:
      return count <= LEAF_SPACE / l.stride / 2;
    case Branch:
      return count <= ((BRANCH_SPACE - 1) / BRANCH_STRIDE - 1) / 2;
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

  BTrie::Siblings
  operator|(BTrie::Siblings s, BTrie::Siblings t)
  {
    using UL = std::underlying_type<BTrie::Siblings>::type;
    return BTrie::Siblings(static_cast<UL>(s) | static_cast<UL>(t));
  }

  BTrie::Siblings
  operator&(BTrie::Siblings s, BTrie::Siblings t)
  {
    using UL = std::underlying_type<BTrie::Siblings>::type;
    return BTrie::Siblings(static_cast<UL>(s) & static_cast<UL>(t));
  }

  BTrie::Siblings &
  operator|=(BTrie::Siblings &s, BTrie::Siblings t)
  {
    s = s | t;
    return s;
  }
}
