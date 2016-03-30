#ifndef DB_BTRIE_ITERATOR_H
#define DB_BTRIE_ITERATOR_H

#include <stack>
#include <tuple>

#include "allocator.h"
#include "btrie.h"
#include "trie_iterator.h"

namespace DB {
  /**
   * BTrieIterator
   *
   * A Trie Iterator for traversing data stored in Nested B+-Tries.
   */
  struct BTrieIterator : public TrieIterator {

    /**
     * BTrieIterator::BTrieIterator
     *
     * Construct a brand new trie iterator for a 2-column B+ Trie. The iterator
     * starts at depth -1.
     *
     * @param rootPID The page_id for the root node of the BTrie being iterated over.
     * @param fst     The position of the table's 1st parameter in the ordering.
     * @param snd     The position of the table's 2nd parameter in the ordering. It
     *                is assumed that fst < snd. That is to say, a table must
     *                contain two different query paramaters, with the first
     *                appearing before the second, always. (There is no restriction
     *                on the values held in these columns).
     */
    BTrieIterator(page_id rootPID, int fst, int snd);

    /**
     * BTrieIterator::~BTrieIterator
     *
     * Destructor.
     */
    ~BTrieIterator() override;

    /** Deleted Copy Constructors */
    BTrieIterator(const BTrieIterator &) = delete;
    BTrieIterator & operator = (const BTrieIterator &) = delete;

    /** TrieIterator method overrides */

    void open()               override;
    void up()                 override;
    void next()               override;
    void seek(int searchKey)  override;

    int  key()          const override;
    bool atEnd()        const override;
    bool atValidDepth() const override;

  private:
    const int mFst; // The position of the 1st column in the global ordering.
    const int mSnd; // The position of the 2nd column in the global ordering.

    BTrie * const mDummy; // A dummy node used for storing the leaf node "at
                          // depth -1".

    // A history of leaf pages the iterator has been through to get to the node
    // at its current depth. For each page, we store the offset in that page,
    // and the depth of the node.
    std::stack<std::tuple<page_id, int, int>> mHistory;

    int mCurrDepth; // The actual depth of the iterator
    int mNodeDepth; // The last depth the iterator participated in.

    // Cursor state.
    page_id mPID;
    BTrie * mCurr;
    int     mPos;
  };
}

#endif // DB_BTRIE_ITERATOR_H
