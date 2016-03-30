#include "replacer.h"

#include "frame.h"

namespace DB {
  Replacer::Replacer(Frame *frames, int poolSize)
    : mFrames(frames)
    , mAllNodes(new Node[poolSize]())
    , mFree()
  {
    mFree.fid   = INVALID_FRAME;
    mFree.left  = &mFree;
    mFree.right = &mFree;

    for (int i = 0; i < poolSize; ++i)
      mAllNodes[i].fid = i;
  }

  Replacer::~Replacer() { delete[] mAllNodes; }

  void
  Replacer::framePinned(int fid)
  {
    Node &node = mAllNodes[fid];
    if (node.left) {
      node.left->right = node.right;
      node.right->left = node.left;
      node.left = node.right = nullptr;
    }
  }

  void
  Replacer::frameUnpinned(int fid)
  {
    if (mFrames[fid].isPinned())
      return;

    Node &node = mAllNodes[fid];

    node.left  = &mFree;
    node.right = mFree.right;

    mFree.right      = &node;
    node.right->left = &node;
  }

  int
  Replacer::pickVictim() const
  {
    return mFree.left->fid;
  }
}
