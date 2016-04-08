#include "view.h"

#include <cstring>

#include "db.h"
#include "ftrie.h"
#include "trie.h"

namespace DB {
  View::View(int width)
    : mWidth   ( width )
    , mRootPID ( FTrie::leaf(width) )
    , mTrie    ( FTrie::load(mRootPID) )
  {}

  View::~View()
  {
    mTrie = nullptr;
    Global::BUFMGR->unpin(mRootPID);
  }

  void View::insert(int *data) { logTxn(FTrie::Insert, data); }
  void View::remove(int *data) { logTxn(FTrie::Delete, data); }

  void
  View::clear()
  {
  }

  void
  View::logTxn(FTrie::TxnType msg, int *data)
  {
    // Build a buffer containing a single transaction.
    int *txns = new int[1 + mTrie->txnSize()]();
    txns[0]   = 1;
    auto txn  = (FTrie::Transaction *)&txns[1];

    txn->message = msg;
    memmove(txn->data, data, mWidth * sizeof(int));

    auto diff = FTrie::flush(mRootPID, {.sibs = NO_SIBS}, txns);
    delete[] txns;

    if (diff.prop == PROP_SPLIT) {
      Global::BUFMGR->unpin(mRootPID);

      mRootPID = FTrie::branch(mWidth, mRootPID, *diff.newSlots);
      mTrie    = FTrie::load(mRootPID);

      delete diff.newSlots;
    } else if (mTrie->isEmpty()            &&
               mTrie->getType()  == Branch &&
               mTrie->txns(0)[0] == 0) {

      page_id newRoot = mTrie->slot(0)[-1];

      Global::BUFMGR->unpin(mRootPID);
      Global::BUFMGR->bfree(mRootPID);

      mRootPID = newRoot;
      mTrie    = FTrie::load(mRootPID);
    }
  }
}
