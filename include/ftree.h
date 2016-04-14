#ifndef DB_FTRIE_H
#define DB_FTRIE_H

#include "allocator.h"
#include "trie.h"

#include <memory>
#include <vector>

namespace DB {
  /**
   * FTree
   *
   * Implementation of Nested Fractal Trees. Unlike the Nested B+ Trie
   * implementation, this one handles arbitrary depth, and relies solely upon
   * splitting and merging to maintain balance (no redistribution).
   *
   * Furthermore, rather than actually nesting indices in memory, this
   * implementation is of a multi-key index (where entire records are stored as
   * pivots/partitioning keys). This improves safe efficiency at larger depths.
   *
   * If the FTree node has N bytes of space for data, then sqrt(N) of it will be
   * used for pointers to children, and the remaining space is used for
   * transactions.
   *
   * Child pointers and partitioning keys are stored together and the beginning
   * of the page, growing forwards. Transactions grow backwards from the end of
   * the page.
   *
   * Transaction space is shared evenly amongst children.
   */
  struct FTree {

    /**
     * FTree::TxnType
     *
     * Tag for transactions.
     */
    enum TxnType : unsigned int { Insert, Delete };

    /**
     * FTree::Transaction
     *
     * Structure of transactions waiting in a buffer on a node to be flushed to
     * a child. Stores the operation to perform (the "message") as well as the
     * associated record. A transaction for a node whose width is `w` must have
     * an associated record that is also `w` in length.
     */
    struct Transaction {
      TxnType message;
      int data[1];
    };

    /** Convenient type alias to avoid a long type signature. */
    using NewSlots = std::vector<int> *;

    /**
     * FTree::Diff
     *
     * A log of changes a child node had to make when servicing the flushed
     * transactions.
     */
    struct Diff {
      Propagate prop;      // What type of operation occurred at the child node.
      union {
        Siblings sib;      // Which sibling was merged with.
        NewSlots newSlots; // Which new children were created.
      };
    };

    /**
     * FTree::leaf
     *
     * Create a new leaf node.
     *
     * @param width The width (in number of columns) of records represented by
     *              paths in this trie.
     * @return THe page ID of the new leaf.
     */
    static page_id leaf(int width);

    /**
     * FTree::branch
     *
     * Create a branch node containing all the partitions provided
     *
     * @param width The width of the records represented by paths in this trie.
     * @param leftPID The page ID of the left most page (not belonging in a
     *                slot).
     * @param slots The slots that should be reachable in this trie.
     * @return The page ID of the constructed branch node. Note that depending
     *         on the size of the slots vector, we may need to construct
     *         multiple layers of branches.
     */
    static page_id branch(int width, page_id leftPID,
                          std::vector<int> slots);

    /**
     * FTree::load
     *
     * @param nid The Page ID of the node to load.
     * @return The pointer to the page that was loaded, casted as an FTree
     *         pointer.
     */
    static FTree *load(page_id nid);

    /**
     * FTree::flush
     *
     * Log transactions to occur on this node and/or its children.
     *
     * @param nid The page_id of this node.
     * @param family Information pertaining to which neighbours of this node are
     *               siblings (i.e. share the same parent).

     * @param txns The buffer of transactions to log. The first index holds the
     *             number of transactions, and the remaining space is occupied
     *             by those transactions. It is assumed that the buffer is big
     *             enough to accommodate the number of transactions it says it
     *             can, and that they are in sorted order (w.r.t. the index
     *             key).
     * @return An account of whether the data structure was merged or split
     *         because of the flush. Parent nodes may use this information to
     *         adjust their partitioning keys.
     */
    static Diff flush(page_id nid, Family family, int *txns);

    /**
     * FTree::debugPrint
     *
     * Print the contents of the tree rooted at this node.
     *
     * @param nid The page ID of the node to print.
     */
    static void debugPrint(page_id nid);

    /**
     * FTree::debugPrintTxns
     *
     * Print the contents of the transaction buffer in a human-readable format.
     *
     * @param txns The buffer, with the number of transactions held at the first
     *             index.
     * @param txnSize Size (in number of integers) per transaction.
     * @param width Size (in number of integers) of data segment of transaction.
     */
    static void debugPrintTxns(int *txns, int txnSize, int width);

    /**
     * FTree::debugPrintKey
     *
     * Format and print a key of the given length.
     *
     * @param key Pointer to the key.
     * @param width Length of the key.
     */
    static void debugPrintKey(int *key, int width);

    /**
     * FTree::split
     *
     * Share the contents of this node between itself and a new neighbour to the
     * right.
     *
     * @param pid The page ID of this node.
     * @param key Pointer that is filled with the key that partitions the old
     *            and new nodes.
     * @return The page_id of the new neighbour.
     */
    page_id split(page_id pid, int *key);

    /**
     * FTree::merge
     *
     * Append the slots and transaction buffers from the other node onto the end
     * of this node's. Pre conditions include: The types of this node and that
     * node must match, that must be the right neighbour of this, and this node
     * must have enough space for the data from both nodes.
     *
     * @param nid  The page_id of this node.
     * @param that The node to merge into this one.
     * @param part Pointer to the key that partitions these two nodes (When
     *             merging branches, this needs to be added back in).
     */
    void merge(page_id nid, FTree *that, int *part);

    /**
     * FTree::isEmpty
     *
     * Check if the node has no slots filled.
     */
    bool isEmpty() const;

    /**
     * FTree::isFull
     *
     * Check if the node is full (all slots filled)
     */
    bool isFull() const;

    /**
     * FTree::isUnderOccupied
     *
     * Check if more than half the nodes are empty.
     */
    bool isUnderOccupied() const;

    /**
     * FTree::getType
     *
     * @return the node type.
     */
    NodeType getType() const;

    /**
     * FTree::slot
     *
     * @param index The index of the slot
     * @return a pointer to the slot.
     */
    int *slot(int index);

    /**
     * FTree::txns
     *
     * @param index The index of the slot
     * @return a pointer to the transaction buffer corresponding to the slot.
     */
    int *txns(int index);

    /**
     * (private) FTree::txnSize
     *
     * @return The size of a single transaction message in this node, (in terms
     *         of number of integers).
     */
    inline int txnSize() const { return TXN_HEADER_SIZE + width; }

  private:
    static const int SPACE;
    static const int SLOT_SPACE;
    static const int TXN_SPACE;
    static const int TXN_HEADER_SIZE;

    /**
     * FTree::FTree
     *
     * Default constructor. Made private because this class is not intended to
     * be constructed on the stack or the heap, but is used to "add structure"
     * to pages returned by the Buffer Manager.
     */
    FTree() = default;

    NodeType type;
    int      count;
    int      width;
    page_id  prev, next;
    int      data[1];

    /**
     * (private) FTree::cmpKey
     *
     * Compare two keys using a lexicographic ordering.
     *
     * @param key1 Pointer to the first key.
     * @param key2 Pointer to the second key.
     * @param width Number of columns in a key.
     * @return -1 if key1 < key, 0 if key1 = key2 and +1 if key1 > key2
     */
    static int cmpKey(int *key1, int *key2, int width);

    /**
     * (private) FTree::findKey
     *
     * @param key Pointer to the key to search for.
     * @param from An index to start the search from (inclusive). This defaults
     *             to 0, meaning all slots are searched.
     * @return The index of the slot in the node corresponding to the smallest
     *         key greater than or equal to the one provided.
     */
    int findKey(int *key, int from = 0);

    /**
     * (private) FTree::findNewSlot
     *
     * Like findKey, but looking through the NewSlot data structure for the slot
     * whose key is the smallest that is greater than or equal to the target.
     *
     * @param slots The slots to search in.
     * @param key The key to search for.
     * @return The index into the slots vector where the search ended.
     */
    int findNewSlot(const NewSlots &slots, int *key);

    /**
     * (private) FTree::findTxn
     *
     * Like findKey, but looking through transactions for the transaction whose
     * key is the smallest that is greater than or equal to the target.
     *
     * @param txns The buffer holding transactions (First index holds size of
     *             buffer).
     * @param width The size of a single record in the transaction.
     * @param from The index to start the search from (inclusive).
     * @param key Pointer to the search key.
     * @return The index into the transaction buffer where the search ended.
     */
    static int findTxn(int *txns, int width, int from, int *key);

    /**
     * (private) FTree::mergeTxns
     *
     * Merge two sorted buffers of transactions.
     *
     * @param existing The buffer of existing transactions. This buffer should
     *                 contain its count at the first index.
     * @param incoming Buffer of new transactions (The count of transactions in
     *                 this buffer should *not* appear at the first index, but
     *                 is instead provided separately).
     * @param incCount The number of new transactions.
     * @param txnSize The size (in number of integers) of a single
     *                transaction. The size is assumed to be the same across
     *                both buffers.
     * @param width The number of columns (integers) in a record stored in a
     *              single transaction.
     * @return A pointer to a new buffer containing the merge result, (and its
     *         size at position 0).
     */
    static int *mergeTxns(int *existing, int *incoming, int incCount,
                          int width);

    /**
     * (private) FTree::makeRoom
     *
     * Make a space of appropriate size at the given index to fit an extra
     * slot. This function does not make sure there is enough room.
     *
     * @param index The position at which to make room.
     * @param size  Number of slots to make room for (defaults to 1).
     */
    void makeRoom(int index, int size = 1);

    /**
     * (private) FTree::capacity
     *
     * @return The number of slots this node can hold.
     */
    int capacity() const;

    /**
     * (private) FTree::slotSpace
     *
     * @return The number of integers available for new slots in the data
     *         segment.
     */
    int slotSpace() const;

    /**
     * (private) FTree:: txnSpacePerChild
     *
     * The size (in number of integers) of the transaction buffer for a single
     * child node.
     */
    int txnSpacePerChild() const;

    /**
     * (private) FTree::txnsPerChild
     *
     * @return The number of transactions that can be stored per child node.
     */
    int txnsPerChild() const;

    /**
     * (private) FTree::stride
     *
     * @return The size of a single slot, in number of integers.
     */
    inline int stride() const
    {
      return type == Leaf
        ? width
        : width + 1;
    }
  };
}

#endif // DB_FTRIE_H
