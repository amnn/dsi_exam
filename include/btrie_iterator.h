#ifndef DB_BTRIE_ITERATOR_H
#define DB_BTRIE_ITERATOR_H

#include <map>
#include <utility>

#include "allocator.h"
#include "btrie.h"
#include "trie_iterator.h"

namespace DB {
  struct BTrieIterator : public TrieIterator {

    /**
     * BTrieIterator::BTrieIterator
     *
     * Construct a brand new trie iterator for a 2-column B+ Trie. The iterator
     * starts at depth -1.
     *
     * @param rootPID The page_id for the root node of the BTrie being iterated over.
     * @param fst     The position of the table's 1st parameter in the ordering.
     * @param snd     The position of the table's 2nd parameter in the ordering.
     *                It is assumed that fst < snd. That is to say, a table must
     *                contain two different paramaters, with the first appearing
     *                before the second, always.
     */
    BTrieIterator(page_id rootPID, int fst, int snd);

    /**
     * BTrieIterator::~BTrieIterator
     *
     * Destructor.
     */
    ~BTrieIterator();

    /** Deleted Copy Constructors */
    BTrieIterator(const BTrieIterator &) = delete;
    BTrieIterator & operator = (const BTrieIterator &) = delete;

    /** TrieIterator method overrides */

    void open() override;
    void up()   override;
    void next() override;

    int  key()   const override;
    bool atEnd() const override;

  private:
    const int mFst;
    const int mSnd;

    BTrie * const mDummy;

    std::map<int, std::pair<page_id, int>> mPath;

    int     mCurrDepth;
    int     mNodeDepth;
    page_id mPID;
    BTrie * mCurr;
    int     mPos;

    /**
     * (private) BTrieIterator::atValidDepth
     *
     * @return true iff The table backing this iterator has a column at the at
     *         the depth the iterator is currently at. (i.e. If the iterator
     *         should participate in a join at this depth).
     */
    bool atValidDepth() const;
  };
}

#endif // DB_BTRIE_ITERATOR_H
