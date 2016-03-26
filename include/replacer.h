#ifndef DB_REPLACER_H
#define DB_REPLACER_H

#include "frame.h"

namespace DB {
  /**
   * Replacer
   *
   * Private class to BufMgr, representing the eviction policy of the
   * cache. This implementation is of LRU eviction.
   */
  struct Replacer {
    Replacer(Frame *frames, int poolSize);

    void framePinned(int fid);
    void frameUnpinned(int fid);

    int pickVictim() const;

  private:
    struct Node {
      int fid;
      Node *left, *right;
    };

    Frame * mFrames;
    int     mPoolSize;
    Node  * mAllNodes;
    Node    mFree;
  };
}

#endif // DB_REPLACER_H
