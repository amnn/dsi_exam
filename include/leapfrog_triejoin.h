#ifndef DB_LEAPFROG_TRIEJOIN_H
#define DB_LEAPFROG_TRIEJOIN_H

#include <memory>
#include <vector>

#include "trie_iterator.h"

namespace DB {
  /**
   * LeapFrogTrieJoin
   *
   * Implementation of LeapFrog Trie Join, with the result un-materialised, and
   * exposed through a Trie Iterator interface.
   */
  struct LeapFrogTrieJoin : public TrieIterator {

    /**
     * LeapFrogTrieJoin::LeapFrogTrieJoin
     *
     * Perform an equijoin over all the provided iterators. The join is
     * performed according to the global ordering: All iterates are joined on
     * parameter 0, then 1, and so on until they have been joined on every
     * column in the join.
     *
     * @param joinSize The size of a record in the join. The records in the join
     *                 will be records in the prefix of length `joinSize` of the
     *                 global ordering.
     * @param iters The iterators to join over. It must be the case that for
     *              every parameter that appears in the join, at least one of
     *              the iterators has a column at that position.
     */
    LeapFrogTrieJoin(int joinSize, std::vector<TrieIterator::Ptr> &&iters);

    /** Deleted Copy Constructors */
    LeapFrogTrieJoin(const LeapFrogTrieJoin &) = delete;
    LeapFrogTrieJoin & operator = (const LeapFrogTrieJoin &) = delete;

    /** TrieIterator method overrides */
    void open()               override;
    void up()                 override;
    void next()               override;
    void seek(int searchKey)  override;

    int  key()          const override;
    bool atEnd()        const override;
    bool atValidDepth() const override;

  private:
    const int mJoinSize;

    int  mDepth;
    int  mNextIter;
    int  mKey;
    bool mAtEnd;

    std::vector<TrieIterator::Ptr> mActiveIters;
    std::vector<TrieIterator::Ptr> mDormantIters;

    /**
     * (private) LeapFrogTrieJoin::init();
     *
     * Initialise the state of the iterator. Usually called after the level of
     * the iterator has changed.
     */
    void init();

    /**
     * (private) LeapFrogTrieJoin::search();
     *
     * Advance all the active iterators until their keys all match, or one of
     * them is expended.
     */
    void search();
  };
}

#endif // DB_LEAPFROG_TRIEJOIN_H
