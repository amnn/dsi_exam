#ifndef DB_TRIE_ITERATOR_H
#define DB_TRIE_ITERATOR_H

namespace DB {
  /**
   * TrieIterator
   *
   * Interface used by LeapFrog Trie Join algorithm to traverse tables, level by
   * level.
   */
  struct TrieIterator {
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
  };
}

#endif // DB_TRIE_ITERATOR
