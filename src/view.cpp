#include "view.h"

#include <cstring>

#include "db.h"
#include "ftree.h"
#include "trie.h"

namespace DB {
  View::View(int width)
    : mWidth   ( width )
    , mRootPID ( FTree::leaf(width) )
    , mTree    ( FTree::load(mRootPID) )
  {}

  View::~View()
  {
    mTree = nullptr;
    Global::BUFMGR->unpin(mRootPID);
  }

  void View::insert(int *data) { logTxn(FTree::Insert, data); }
  void View::remove(int *data) { logTxn(FTree::Delete, data); }

  void
  View::clear()
  {
  }

  void
  View::logTxn(FTree::TxnType msg, int *data)
  {
    // Build a buffer containing a single transaction.
    int *txns = new int[1 + mTree->txnSize()]();
    txns[0]   = 1;
    auto txn  = (FTree::Transaction *)&txns[1];

    txn->message = msg;
    memmove(txn->data, data, mWidth * sizeof(int));

    auto diff = FTree::flush(mRootPID, {.sibs = NO_SIBS}, txns);
    delete[] txns;

    if (diff.prop == PROP_SPLIT) {
      Global::BUFMGR->unpin(mRootPID);

      mRootPID = FTree::branch(mWidth, mRootPID, *diff.newSlots);
      mTree    = FTree::load(mRootPID);

      delete diff.newSlots;
    } else if (mTree->isEmpty()            &&
               mTree->getType()  == Branch &&
               mTree->txns(0)[0] == 0) {

      page_id newRoot = mTree->slot(0)[-1];

      Global::BUFMGR->unpin(mRootPID);
      Global::BUFMGR->bfree(mRootPID);

      mRootPID = newRoot;
      mTree    = FTree::load(mRootPID);
    }
  }
}
