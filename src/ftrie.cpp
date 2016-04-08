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
  FTrie::branch(int width, page_id leftPID, std::vector<BranchSlot> slots)
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

    std::vector<BranchSlot> spillOver;
    page_id bid;
    auto branch = freshBranch(bid, leftPID);

    leftPID = bid; // Stash the first branch for the recursive call.

    for(const auto &slot : slots) {
      int partition = std::get<0>(slot);
      page_id child = std::get<1>(slot);

      if (branch->isFull()) {
        page_id newBID;
        auto newBranch = freshBranch(newBID, child, bid);

        // add nextBranch to the spillOver;
        spillOver.emplace_back(std::make_pair(partition, newBID));

        // replace bid with newBID
        Global::BUFMGR->unpin(bid);
        bid    = newBID;
        branch = newBranch;
      } else {
        branch->slot(branch->count)[0] = partition;
        branch->slot(branch->count)[1] = child;
        branch->count++;
      }
    }

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
    int     pid  = nid; // Currently pinned node.
    FTrie * node = load(pid);

    // Cache some constants
    const int   TC  = txns[0];          // Transaction Count
    const int   TS  = node->txnSize();  // Transaction Size
    const int   W   = node->width;      // Record Width
    const auto  NT  = node->type;       // Node Type

    std::cout << "txns: "; debugPrintTxns(txns, TS, W);

    // Set up a place for splits to be recorded.
    const NewSlots newNbrs = new std::vector< BranchSlot >();

    // When dealing with transactions, we may need to load one of these new
    // pages, so we keep track of that index, and the position within the pinned
    // node too.
    int nbr = -1;
    int pos = -1;

    // A function which searches for the key in the available nodes.
    auto seekKey = [nid, &pid, &node, &newNbrs, &nbr, &pos](int key) {
      // Find the appropriate node to search in.
      nbr = findNewSlot(newNbrs, key);

      // Load it and unpin the old one if need be.
      int toPin = nbr == 0 ? nid : std::get<1>((*newNbrs)[nbr - 1]);
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
      if (W == 1) {
        std::cout << "Bottom" << std::endl;
        // We are at the very bottom of the tree, we must apply all the
        // transactions.
        while (t < TC) {
          auto txn = (Transaction *)&txns[TS * t + 1];
          int  key = txn->data[0];

          std::cout << "Key: " << key << std::endl;
          seekKey(key);

          switch (txn->message) {
          case Insert:
            // Record is already present.
            if (pos < node->count &&
                node->slot(pos)[0] == key)
              break;

            if (node->isFull()) {
              // We must split and try again.
              std::cout << "SPLIT" << std::endl;
              int partKey;
              page_id newNbr = node->split(pid, partKey);
              newNbrs->emplace(std::next(newNbrs->begin(), nbr),
                               std::make_pair(partKey, newNbr));
              continue;
            }

            // Perform the insertion.
            node->makeRoom(pos, 1);
            node->slot(pos)[0] = key;
            break;

          case Delete:
            // Record is not present.
            if (pos == node->count ||
                node->slot(pos)[0] != key)
              break;

            // Perform the deletion.
            node->makeRoom(pos + 1, -1);
            break;
          }

          t++;
        }
        break;
      }

      // We are at an intermediary leaf
      std::cout << "Leaf" << std::endl;
      while (t < TC) {
        auto txn = (Transaction *)&txns[TS * t + 1];
        int  key = txn->data[0];
        seekKey(key);

        if (pos == node->count ||
            node->slot(pos)[0] != key) {
          // We will need to insert a new slot here.
          if (node->isFull()) {
            // We must split the node
            int partKey;
            page_id newNbr = node->split(pid, partKey);
            newNbrs->emplace(std::next(newNbrs->begin(), nbr),
                             std::make_pair(partKey, newNbr));
            continue;
          }

          page_id lid = FTrie::leaf(W - 1);
          node->makeRoom(pos, 1);
          node->slot(pos)[0] = key;
          node->slot(pos)[1] = lid;
        }

        // Find the index of the first transaction not going to the same node.
        int u = findTxn(txns, TS, t, key + 1);

        // Build the new transaction buffer.
        int *childTxns = new int[1 + (u - t) * (TS - 1)]();
        childTxns[0]   = u - t;
        for (int i = t; i < u; ++i) {
          auto fromTxn = (Transaction *)&txns[TS * i + 1];
          auto toTxn   = (Transaction *)&childTxns[(i - t) * (TS - 1) + 1];

          toTxn->message = fromTxn->message;
          memmove(toTxn->data, fromTxn->data + 1, (W - 1) * sizeof(int));
        }

        // Flush it
        page_id subPID = node->slot(pos)[1];
        auto childDiff = flush(subPID, {.sibs = NO_SIBS}, childTxns);
        delete[] childTxns;

        // Deal with the Diff
        if (childDiff.prop == PROP_SPLIT) {
          std::cout << "child split" << std::endl;
          node->slot(pos)[1] =
            FTrie::branch(W-1, node->slot(pos)[1],
                          *childDiff.newSlots);

          delete childDiff.newSlots;
        } else {
          std::cout << "child empty?" << std::endl;
          // Check whether the child is empty.
          FTrie *sub = load(subPID);
          if (sub->isEmpty()) {
            switch (sub->type) {
            case Leaf:
              // Delete this sub-index entirely.
              Global::BUFMGR->unpin(subPID);
              Global::BUFMGR->bfree(subPID);
              node->makeRoom(pos + 1, -1);
              break;

            case Branch:
              // We cannot collapse if there are waiting transactions.
              if (sub->txns(0)[0] > 0) {
                Global::BUFMGR->unpin(subPID);
              } else {
                // Replace the branch with its only child.
                node->slot(pos)[1] = sub->slot(0)[-1];

                Global::BUFMGR->unpin(subPID);
                Global::BUFMGR->bfree(subPID);
              }
              break;
            }
          } else {
            Global::BUFMGR->unpin(subPID);
          }

        }
        t = u;
      }
      break;
    case Branch:
      std::cout << "Branch" << std::endl;
      while (t < TC) {
        auto txn = (Transaction *)&txns[TS * t + 1];
        int  key = txn->data[0];
        std::cout << "Key: " << key << std::endl;

        seekKey(key);

        // Find all the transactions being sent to this child.
        int u;
        if (pos == node->count) {
          u = TC;
        } else {
          int pivot = node->slot(pos)[0];
          u = findTxn(txns, TS, t, pivot + 1);
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
          std::cout << "child flush" << std::endl;
          // If not, flush all the transactions to the child.
          Family childFamily {.sibs = NO_SIBS};
          if (pos > 0) {
            childFamily.sibs   |= LEFT_SIB;
            childFamily.leftKey = node->slot(pos - 1)[0];
          }

          if (pos < node->count) {
            childFamily.sibs    |= RIGHT_SIB;
            childFamily.rightKey = node->slot(pos)[0];
          }

          auto childDiff = flush(node->slot(pos)[-1], childFamily, mergedTxns);
          node->txns(pos)[0] = 0; // We have flushed them.
          delete[] mergedTxns;

          // Deal with the diff.
          if (childDiff.prop == PROP_SPLIT) {
            std::cout << "child split" << std::endl;
            auto it = childDiff.newSlots->begin();

            while (it != childDiff.newSlots->end()) {
              int     slotKey = std::get<0>(*it);
              page_id slotPID = std::get<1>(*it);
              seekKey(slotKey);

              if (node->isFull()) {
                // We must split the node
                std::cout << "SPLIT" << std::endl;
                int partKey;
                page_id newNbr = node->split(pid, partKey);
                newNbrs->emplace(std::next(newNbrs->begin(), nbr),
                                 std::make_pair(partKey, newNbr));
                continue;
              }

              node->makeRoom(pos, 1);
              node->slot(pos)[0] = slotKey;
              node->slot(pos)[1] = slotPID;
              it++;
            }

            delete childDiff.newSlots;
          } else if(childDiff.prop == PROP_MERGE) {
            if (childDiff.sib == RIGHT_SIB) {
              int toFree = node->slot(pos)[+1];

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
      if (node->width == 1) {
        for (int i = 0; i < node->count; ++i)
          std::cout << node->slot(i)[0] << " ";
        std::cout << std::endl;
        break;
      }

      for (int i = 0; i < node->count; ++i)
        std::cout << "[" << node->slot(i)[0]
                  << " -> " << node->slot(i)[1] << "] ";
      std::cout << std::endl;

      for (int i = 0; i < node->count; ++i)
        debugPrint(node->slot(i)[1]);

      break;

    case Branch:
      std::cout << "[" << node->slot(0)[-1] << "] ";
      for (int i = 0; i < node->count; ++i)
        std::cout << "=< " << node->slot(i)[0]
                  << " < [" << node->slot(i)[1] << "] ";

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
      std::cout << (txn->message == Insert ? "+" : "-")
                << "(";
      for (int k = 0; k < width; ++k) {
        if (k != 0) std::cout << " ";
        std::cout << txn->data[k];
      }

      std::cout << ")";
    }
    std::cout << "}" << std::endl;
  }

  page_id
  FTrie::split(page_id pid, int &key)
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
      key         = slot(pivot - 1)[0];
      break;
    case Branch:
      // Move half the children, excluding the pivot key, which we push up.
      memmove(node->slot(0) - 1, slot(pivot + 1) - 1,
              ((count - pivot) * stride() - 1) * sizeof(int));

      // Move half the transaction buffer
      memmove(node->txns(count - pivot - 1),
              txns(count),
              (count - pivot) * txnSpacePerChild() * sizeof(int));

      node->count = count - pivot - 1;
      count       = pivot;
      key         = slot(pivot)[0];
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
  FTrie::merge(page_id nid, FTrie *that, int part)
  {
    switch (type) {
    case Leaf:
      memmove(slot(count), that->slot(0),
              that->count * stride() * sizeof(int));

      count += that->count;
      break;
    case Branch:
      std::cout << "Merge This " << count << std::endl;
      for (int i = 0; i <= count; ++i) {
        debugPrintTxns(txns(i), txnSize(), width);
      }

      std::cout << "Merge That " << that->count << std::endl;
      for (int i = 0; i <= that->count; ++i) {
        debugPrintTxns(that->txns(i), txnSize(), width);
      }

      slot(count)[0] = part;
      memmove(slot(count) + 1, that->slot(0) - 1,
              (1 + that->count * stride()) * sizeof(int));

      memmove(txns(count + that->count + 1),
              that->txns(that->count),
              (that->count + 1) * txnSpacePerChild() * sizeof(int));

      count += that->count + 1;

      std::cout << "Merge Result" << std::endl;
      for (int i = 0; i <= count; ++i) {
        debugPrintTxns(txns(i), txnSize(), width);
      }
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
  FTrie::findKey(int key, int from)
  {
    int lo = from, hi = count;

    while(lo < hi) {
      int m = lo + (hi - lo) / 2;
      int k = slot(m)[0];

      if (key > k) lo = m + 1;
      else         hi = m;
    }

    return hi;
  }

  int
  FTrie::findNewSlot(const NewSlots &slots, int key)
  {
    int lo = 0, hi = slots->size();

    while(lo < hi) {
      int m = lo + (hi - lo) / 2;
      int k = std::get<0>((*slots)[m]);

      if (key > k) lo = m + 1;
      else         hi = m;
    }

    return hi;
  }

  int
  FTrie::findTxn(int *txns, int txnSize, int from, int key)
  {
    int lo = from, hi = txns[0];

    while(lo < hi) {
      int  m   = lo + (hi - lo) / 2;
      auto txn = (Transaction *)&txns[txnSize * m + 1];
      int  k   = txn->data[0];

      if (key > k) lo = m + 1;
      else         hi = m;
    }

    return hi;
  }

  int *
  FTrie::mergeTxns(int *existing, int *incoming, int incCount, int width)
  {
    // Transaction comparator function.
    const auto cmpTxn = [width](Transaction *t1, Transaction *t2) {
      for (int i = 0; i < width; ++i) {
        if      (t1->data[i] < t2->data[i]) return -1;
        else if (t1->data[i] > t2->data[i]) return +1;
      }
      return 0;
    };

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
      int cmp  = cmpTxn(te, ti);

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
