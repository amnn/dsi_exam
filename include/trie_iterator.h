#ifndef DB_TRIE_ITERATOR_H
#define DB_TRIE_ITERATOR_H

#include <memory>

namespace DB {
  /**
   * TrieIterator
   *
   * Interface used by LeapFrog Trie Join algorithm to traverse tables, level by
   * level.
   */
  struct TrieIterator {
    /**
     * TrieIterator::Ptr
     *
     * Type alias for pointers to Trie Iterators, as this is generally how they
     * are passed around.
     */
    using Ptr = std::unique_ptr<TrieIterator>;

    /**
     * TrieIterator::countingScan
     *
     * Count the number of records in the iterator by scanning through it.
     *
     * @param it A pointer to the iterator
     * @param &count A reference to an accumulator that is bumped for every
     *               record in the iterator.
     * @param depth How deep the current iterator is. (How many times can it be
     *              opened)
     */
    static void countingScan(Ptr &it, int &count, int depth);

    /**
     * TrieIterator::~TrieIterator
     *
     * Default (empty) virtual destructor.
     */
    virtual ~TrieIterator() = default;

    /**
     * TrieIterator::open
     *
     * Increase the depth of the iterator.
     */
    virtual void open() = 0;

    /**
     * TrieIterator::up
     *
     * Decrease the depth of the iterator.
     */
    virtual void up()   = 0;

    /**
     * TrieIterator::next
     *
     * Move the iterator one key forward at its current level.
     */
    virtual void next() = 0;

    /**
     * TrieIterator::seek
     *
     * Move the iterator forward to the first key whose value is greater than or
     * equal to the one given.
     *
     * @param key The given key.
     */
    virtual void seek(int key) = 0;

    /**
     * TrieIterator::key
     *
     * @return the key at the current position (and depth).
     */
    virtual int  key()   const = 0;

    /**
     * TrieIterator::atEnd
     *
     * @return true iff the iterator is past the last key at the current level.
     */
    virtual bool atEnd() const = 0;

    /**
     * TrieIterator::atValidDepth
     *
     * @return true iff This iterator has a column at the depth the iterator is
     *         currently at. (i.e. If the iterator should participate in a join
     *         at this depth).
     */
    virtual bool atValidDepth() const = 0;
  };
}

#endif // DB_TRIE_ITERATOR
