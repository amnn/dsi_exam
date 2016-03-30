#ifndef DB_SINGLETON_ITERATOR_H
#define DB_SINGLETON_ITERATOR_H

#include "trie_iterator.h"

namespace DB {
  struct SingletonIterator : public TrieIterator {

    /**
     * SingletonIterator::SingletonIterator
     *
     * Construct a trie iterator which contains only the record
     * [x, y].
     *
     * @param fst The position of the first column.
     * @param x   The value of the record at the first column.
     * @param snd The position of the second column.
     * @param y   The value of the record at the second column.
     */
    SingletonIterator(int fst, int x, int snd, int y);

    /** Deleted Copy Constructors */
    SingletonIterator(const SingletonIterator &) = delete;
    SingletonIterator & operator = (const SingletonIterator &) = delete;

    /** TrieIterator method overrides */

    void open()               override;
    void up()                 override;
    void next()               override;
    void seek(int searchKey)  override;

    int  key()          const override;
    bool atEnd()        const override;
    bool atValidDepth() const override;

  private:
    const int mFst;
    const int mX;
    const int mSnd;
    const int mY;

    int mDepth;
    int mAtEnd = false;

  };
}

#endif // DB_SINGLETON_ITERATOR_H
