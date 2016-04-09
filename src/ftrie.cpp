#include "ftrie.h"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include "db.h"
#include "dim.h"
#include "trie.h"

namespace DB {
  const int FTrie::SPACE  =
    (Dim::PAGE_SIZE - offsetof(FTrie, data)) / sizeof(int);

  const int FTrie::SLOT_SPACE =
    static_cast<int>(std::sqrt(SPACE));

  const int FTrie::TXN_SPACE =
    SPACE - SLOT_SPACE;

  const int FTrie::TXN_HEADER_SIZE =
    offsetof(Transaction, data) / sizeof(int);

  page_id
  FTrie::leaf(int width)
  {
    char *page;
    page_id lid = Global::BUFMGR->bnew(page);
    FTrie *leaf = (FTrie *)page;

    leaf->type  = Leaf;
    leaf->count = 0;
    leaf->prev  = INVALID_PAGE;
    leaf->next  = INVALID_PAGE;
    leaf->width = width;

    Global::BUFMGR->unpin(lid, true);
    return lid;
  }

  page_id
  FTrie::branch(int width, page_id leftPID, std::vector<int> slots)
  {
    if (slots.empty())
      return leftPID;

    auto freshBranch = [width](page_id &bid,
                               page_id leftMostPID,
                               page_id prev = INVALID_PAGE) {
      char *  page;
      bid = Global::BUFMGR->bnew(page);
      auto branch = (FTrie *)page;

      branch->type  = Branch;
      branch->count = 0;
      branch->prev  = prev;
      branch->next  = INVALID_PAGE;
      branch->width = width;

      branch->slot(0)[-1] = leftMostPID;

      return branch;
    };

    std::vector<int> spillOver;
    page_id bid;
    auto branch = freshBranch(bid, leftPID);

    leftPID = bid; // Stash the first branch for the recursive call.

    const int stride = branch->stride();
    for (auto it = slots.begin(); it != slots.end(); it += stride) {
      page_id child = *(it + width);

      if (branch->isFull()) {
        page_id newBID;
        auto newBranch = freshBranch(newBID, child, bid);

        // add nextBranch to the spillOver;
        spillOver.insert(spillOver.end(), it, it + width);
        spillOver.emplace_back(newBID);

        // replace bid with newBID
        Global::BUFMGR->unpin(bid, true);
        bid    = newBID;
        branch = newBranch;
      } else {
        // Copy Partitioning Key
        memmove(branch->slot(branch->count), &*it,
                width * sizeof(int));

        // Copy child PID
        branch->slot(branch->count)[width] = child;
        branch->count++;
      }
    }

    Global::BUFMGR->unpin(bid, true);
    return FTrie::branch(width, leftPID, spillOver);
  }

  FTrie *
  FTrie::load(page_id nid)
  {
    return (FTrie *)Global::BUFMGR->pin(nid);
  }

  FTrie::Diff
  FTrie::flush(page_id nid, Family family, int *txns)
  {
    Diff diff {.prop = PROP_NOTHING };

    int     t    = 0;   // Transaction waiting to be routed.
    page_id pid  = nid; // Currently pinned node.
    FTrie * node = load(pid);

    // Cache some constants
    const int   TC  = txns[0];          // Transaction Count
    const int   TS  = node->txnSize();  // Transaction Size
    const int   W   = node->width;      // Record Width
    const int   SS  = W + 1;            // Slot stride
    const auto  NT  = node->type;       // Node Type

    // Set up a place for splits to be recorded.
    const NewSlots newNbrs = new std::vector<int>();

    // When dealing with transactions, we may need to load one of these new
    // pages, so we keep track of that index, and the position within the pinned
    // node too.
    int nbr = -1;
    int pos = -1;

    // A function which searches for the key in the available nodes.
    auto seekKey = [nid, SS, &pid, &node, &newNbrs, &nbr, &pos](int *key) {
      // Find the appropriate node to search in.
      nbr = node->findNewSlot(newNbrs, key);

      // Load it and unpin the old one if need be.
      page_id toPin = nbr == 0 ? nid : (*newNbrs)[nbr * SS - 1];
      if (toPin != pid) {
        Global::BUFMGR->unpin(pid, true);
        pid  = toPin;
        node = load(pid);
      }

      // Find the position in the node for the key.
      pos = node->findKey(key);
    };

    switch (NT) {
    case Leaf:
      // We are at the very bottom of the tree, we must apply all the
      // transactions.
      while (t < TC) {
        auto txn = (Transaction *)&txns[TS * t + 1];
        int *key = txn->data;
        seekKey(key);

        switch (txn->message) {
        case Insert:
          // Record is already present.
          if (pos < node->count &&
              cmpKey(node->slot(pos), key, W) == 0)
            break;

          if (node->isFull()) {
            // We must split and try again.
            int nextNbr = nbr * SS;
            newNbrs->insert(std::next(newNbrs->begin(), nextNbr), SS, 0);
            page_id newNbr = node->split(pid, &(*newNbrs)[nextNbr]);
            (*newNbrs)[nextNbr + W] = newNbr;
            continue;
          }

          // Perform the insertion.
          node->makeRoom(pos, 1);
          memmove(node->slot(pos), key, W * sizeof(int));
          break;

        case Delete:
          // Record is not present.
          if (pos == node->count ||
              cmpKey(node->slot(pos), key, W) != 0)
            break;

          // Perform the deletion.
          node->makeRoom(pos + 1, -1);
          break;
        }

        t++;
      }
      break;
    case Branch:
      while (t < TC) {
        auto txn = (Transaction *)&txns[TS * t + 1];
        int *key = txn->data;
        seekKey(key);

        // Find all the transactions being sent to this child.
        int u;
        if (pos == node->count) {
          u = TC;
        } else {
          int *pivot = new int[W];
          memmove(pivot, node->slot(pos), W * sizeof(int));
          pivot[W - 1]++;

          u = findTxn(txns, W, t, pivot);
          delete[] pivot;
        }

        // Merge the new and existing transactions.
        int *mergedTxns =
          mergeTxns(node->txns(pos), &txns[TS * t + 1], u - t, W);

        // Just put them back into the buffer, if we can.
        if (mergedTxns[0] <= node->txnsPerChild()) {
          memmove(node->txns(pos), mergedTxns,
                  (1 + mergedTxns[0] * node->txnSize()) * sizeof(int));

          delete[] mergedTxns;
        } else {
          // If not, flush all the transactions to the child.
          Family childFamily {.sibs = NO_SIBS};
          if (pos > 0) {
            childFamily.sibs   |= LEFT_SIB;
            childFamily.leftKey = node->slot(pos - 1);
          }

          if (pos < node->count) {
            childFamily.sibs    |= RIGHT_SIB;
            childFamily.rightKey = node->slot(pos);
          }

          auto childDiff = flush(node->slot(pos)[-1], childFamily, mergedTxns);
          node->txns(pos)[0] = 0; // We have flushed them.
          delete[] mergedTxns;

          // Deal with the diff.
          if (childDiff.prop == PROP_SPLIT) {
            auto it = childDiff.newSlots->begin();

            while (it != childDiff.newSlots->end()) {
              int *   slotKey = &*it;
              page_id slotPID = *(it + W);
              seekKey(slotKey);

              if (node->isFull()) {
                // We must split the node
                int nextNbr = nbr * SS;
                newNbrs->insert(std::next(newNbrs->begin(), nextNbr), SS, 0);
                page_id newNbr = node->split(pid, &(*newNbrs)[nextNbr]);
                (*newNbrs)[nextNbr + W] = newNbr;
                continue;
              }

              node->makeRoom(pos, 1);
              memmove(node->slot(pos), slotKey, W * sizeof(int));
              node->slot(pos)[W] = slotPID;
              it += SS;
            }

            delete childDiff.newSlots;
          } else if(childDiff.prop == PROP_MERGE) {
            if (childDiff.sib == RIGHT_SIB) {
              int toFree = node->slot(pos)[W];

              node->makeRoom(pos + 1, -1);
              Global::BUFMGR->bfree(toFree);
            } else if (childDiff.sib == LEFT_SIB) {
              int toFree = node->slot(pos)[-1];

              // Adopt the transaction buffer from the sibling, before it gets
              // deleted.
              memmove(node->txns(pos), node->txns(pos - 1),
                      node->txnSpacePerChild() * sizeof(int));

              node->makeRoom(pos, -1);
              Global::BUFMGR->bfree(toFree);
            }
          }
        }

        t = u;
      }
      break;
    }

    if (!newNbrs->empty()) {
      diff.prop = PROP_SPLIT;
      diff.newSlots = newNbrs;
      Global::BUFMGR->unpin(pid, true);
      return diff;
    } else {
      delete newNbrs;
    }

    if (!node->isUnderOccupied()) {
      Global::BUFMGR->unpin(pid, true);
      return diff;
    }

    if (family.sibs & LEFT_SIB) {
      page_id lid = node->prev;
      FTrie *left = load(lid);

      if (!left->isUnderOccupied()) {
        Global::BUFMGR->unpin(lid);
      } else {
        diff.prop = PROP_MERGE;
        diff.sib  = LEFT_SIB;

        left->merge(lid, node, family.leftKey);

        Global::BUFMGR->unpin(lid, true);
        Global::BUFMGR->unpin(pid);
        return diff;
      }
    }

    if (family.sibs & RIGHT_SIB) {
      page_id rid  = node->next;
      FTrie *right = load(rid);

      if (!right->isUnderOccupied()) {
        Global::BUFMGR->unpin(rid);
      } else {
        diff.prop = PROP_MERGE;
        diff.sib  = RIGHT_SIB;

        node->merge(pid, right, family.rightKey);

        Global::BUFMGR->unpin(pid, true);
        Global::BUFMGR->unpin(rid);
        return diff;
      }
    }

    Global::BUFMGR->unpin(pid, true);
    return diff;
  }

  void
  FTrie::debugPrint(page_id nid)
  {
    FTrie *node = load(nid);

    std::cout << "========================================\n"
              << (node->type == Leaf ? "L" : "B") << nid << "("
              << "width: " << node->width << ", "
              << "occ: "   << node->count << "/" << node->capacity()
              << ") ";

    std::cout << "<";
    if (node->prev == INVALID_PAGE)
      std::cout << "x";
    else
      std::cout << node->prev;

    std::cout << "|";

    if (node->next == INVALID_PAGE)
      std::cout << "x";
    else
      std::cout << node->next;

    std::cout << ">"
    << "\n----------------------------------------"
              << std::endl;

    switch (node->type) {
    case Leaf:
      for (int i = 0; i < node->count; ++i) {
        debugPrintKey(node->slot(i), node->width);
        std::cout << " ";
      }
      std::cout << std::endl;
      break;

    case Branch:
      std::cout << "[" << node->slot(0)[-1] << "] ";

      for (int i = 0; i < node->count; ++i) {
        std::cout << " =< ";
        debugPrintKey(node->slot(i), node->width);
        std::cout<< " < [" << node->slot(i)[node->width] << "] ";
      }

      std::cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;
      for (int i = 0; i <= node->count; ++i)
        debugPrintTxns(node->txns(i), node->txnSize(), node->width);

      for (int i = 0; i <= node->count; ++i)
        debugPrint(node->slot(i)[-1]);

      break;
    }

    Global::BUFMGR->unpin(nid);
  }

  void
  FTrie::debugPrintTxns(int *txns, int txnSize, int width)
  {
    int txnCount = txns[0];

    std::cout << txnCount << " * " << txnSize << " {";
    for (int j = 0; j < txnCount; ++j) {
      if (j != 0) std::cout << " ";

      auto txn = (Transaction *)&txns[1 + j * txnSize];
      std::cout << (txn->message == Insert ? "+" : "-");
      debugPrintKey(txn->data, width);
    }
    std::cout << "}" << std::endl;
  }

  void
  FTrie::debugPrintKey(int *key, int width)
  {
    std::cout << "(";
    for (int i = 0; i < width; ++i) {
      if(i != 0) std::cout << " ";
      std::cout << key[i];
    }
    std::cout << ")";
  }

  page_id
  FTrie::split(page_id pid, int *key)
  {
    // Allocate a new page
    char *page;
    page_id nid = Global::BUFMGR->bnew(page);
    FTrie *node = (FTrie *)page;
    node->type  = type;
    node->width = width;

    int pivot = count / 2;

    switch (type) {
    case Leaf:
      // Move half the records.
      memmove(node->slot(0), slot(pivot),
              (count - pivot) * stride() * sizeof(int));

      // Move half the transaction buffer
      memmove(node->txns(count - pivot - 1),
              txns(count - 1),
              (count - pivot) * txnSpacePerChild() * sizeof(int));

      node->count = count - pivot;
      count       = pivot;

      // Make a note of the pivot key
      memmove(key, slot(pivot - 1), width * sizeof(int));
      break;
    case Branch:
      // Move half the children, excluding the pivot key, which we push up.
      memmove(node->slot(0) - 1, slot(pivot + 1) - 1,
              ((count - pivot - 1) * stride() + 1) * sizeof(int));

      // Move half the transaction buffer
      memmove(node->txns(count - pivot - 1),
              txns(count),
              (count - pivot) * txnSpacePerChild() * sizeof(int));

      node->count = count - pivot - 1;
      count       = pivot;

      // Make a note of the pivot key.
      memmove(key, slot(pivot), width * sizeof(int));
      break;
    }

    // Fix the neigbour pointers
    node->prev = pid;
    node->next = next;
    next       = nid;

    if (node->next != INVALID_PAGE) {
      FTrie *nbr = load(node->next);
      nbr->prev = nid;
      Global::BUFMGR->unpin(node->next, true);
    }

    Global::BUFMGR->unpin(nid, true);
    return nid;
  }

  void
  FTrie::merge(page_id nid, FTrie *that, int *part)
  {
    switch (type) {
    case Leaf:
      memmove(slot(count), that->slot(0),
              that->count * stride() * sizeof(int));

      count += that->count;
      break;
    case Branch:
      // Move partitioning key onto the end.
      memmove(slot(count), part, width * sizeof(int));

      // Move slots from neighbour
      memmove(slot(count + 1) - 1, that->slot(0) - 1,
              (1 + that->count * stride()) * sizeof(int));

      // Move transactions from neighbour
      memmove(txns(count + that->count + 1),
              that->txns(that->count),
              (that->count + 1) * txnSpacePerChild() * sizeof(int));

      count += that->count + 1;
      break;
    }

    // Fix neighbour pointers
    next = that->next;

    if (next != INVALID_PAGE) {
      FTrie *newNext = load(next);
      newNext->prev = nid;
      Global::BUFMGR->unpin(next, true);
    }
  }

  bool FTrie::isEmpty()           const { return count == 0; }
  bool FTrie::isFull()            const { return count >= capacity(); }
  bool FTrie::isUnderOccupied()   const { return count < capacity() / 2; }

  NodeType FTrie::getType()       const { return type; }

  int *
  FTrie::slot(int index)
  {
    int *start = type == Leaf ? data : data + 1;
    return start + stride() * index;
  }

  int *
  FTrie::txns(int index)
  {
    int *end = &data[0] + SPACE;
    return end - txnSpacePerChild() * (index + 1);
  }

  int
  FTrie::cmpKey(int *key1, int *key2, int width)
  {
    for (int i = 0; i < width; ++i) {
      if      (key1[i] < key2[i]) return -1;
      else if (key1[i] > key2[i]) return +1;
    }
    return 0;
  }

  int
  FTrie::findKey(int *key, int from)
  {
    int lo = from, hi = count;

    while(lo < hi) {
      int  m = lo + (hi - lo) / 2;
      int *k = slot(m);

      if (cmpKey(key, k, width) > 0)
        lo = m + 1;
      else
        hi = m;
    }

    return hi;
  }

  int
  FTrie::findNewSlot(const NewSlots &slots, int *key)
  {
    int lo = 0, hi = slots->size() / stride();

    while(lo < hi) {
      int  m = lo + (hi - lo) / 2;
      int *k = &((*slots)[m * stride()]);

      if (cmpKey(key, k, width) > 0)
        lo = m + 1;
      else
        hi = m;
    }

    return hi;
  }

  int
  FTrie::findTxn(int *txns, int width, int from, int *key)
  {
    const int txnSize = TXN_HEADER_SIZE + width;
    int lo = from, hi = txns[0];

    while(lo < hi) {
      int   m   = lo + (hi - lo) / 2;
      auto  txn = (Transaction *)&txns[txnSize * m + 1];
      int  *k   = txn->data;

      if (cmpKey(key, k, width) > 0)
        lo = m + 1;
      else
        hi = m;
    }

    return hi;
  }

  int *
  FTrie::mergeTxns(int *existing, int *incoming, int incCount, int width)
  {
    const int txnSize     = TXN_HEADER_SIZE + width;
    const int byteTxnSize = txnSize * sizeof(int);
    const int extCount    = existing[0];

    int *merged = new int[1 + (extCount + incCount) * txnSize]();
    int &k      = merged[0];

    int i = 0, j = 0;
    while (i < extCount && j < incCount) {
      auto tep = existing + txnSize * i + 1;
      auto tip = incoming + txnSize * j;
      auto mep = merged   + txnSize * k + 1;
      auto te  = (Transaction *)tep;
      auto ti  = (Transaction *)tip;
      int cmp  = cmpKey(te->data, ti->data, width);

      if (cmp < 0) {
        memmove(mep, tep, byteTxnSize);
        k++; i++;
      } else if (cmp > 0) {
        memmove(mep, tip, byteTxnSize);
        k++; j++;
      } else {
        // Duplicate Transaction, always keep newer one.
        memmove(mep, tip, byteTxnSize);
        k++; i++; j++;
      }
    }

    // Check for any remaining transactions.
    if (i < extCount) {
      memmove(merged   + txnSize * k + 1,
              existing + txnSize * i + 1,
              (extCount - i) * byteTxnSize);

      k += extCount - i;
    } else if (j < incCount) {
      memmove(merged   + txnSize * k + 1,
              incoming + txnSize * j,
              (incCount - j) * byteTxnSize);

      k += incCount - j;
    }

    return merged;
  }

  void
  FTrie::makeRoom(int index, int size)
  {
    // Move slots
    memmove(slot(index + size), slot(index),
            (count - index) * stride() * sizeof(int));

    if (type == Branch) {
      // Move transactions
      memmove(txns(count + size), txns(count),
              (count - index + 1) * txnSpacePerChild() * sizeof(int));

      // Initialise counts of new transaction buffers.
      for (int i = index; i < index + size; ++i) {
        txns(i)[0] = 0;
      }
    }

    count += size;
  }

  int FTrie::capacity() const { return slotSpace() / stride(); }

  int
  FTrie::slotSpace() const
  {
    switch (type) {
    case Leaf:
      return SPACE;
    case Branch:
      return SLOT_SPACE - 1;
    default:
      throw std::runtime_error("slotSpace: Unrecognised Node Type");
    }
  }

  int
  FTrie::txnSpacePerChild() const
  {
    switch (type) {
    case Leaf:
      return 0;
    case Branch:
      // Branches have one more child pointer than they have slots.
      return TXN_SPACE / (capacity() + 1);
    default:
      throw std::runtime_error("txnSpacePerChild: Unrecognised Node Type");
    }
  }

  int
  FTrie::txnsPerChild() const
  {
    return (txnSpacePerChild() - 1) / txnSize();
  }
}
