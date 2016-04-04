#ifndef DB_BTRIE_H
#define DB_BTRIE_H

#include <cstddef>
#include <cstring>
#include <functional>
#include <stdexcept>

#include "allocator.h"
#include "db.h"
#include "trie.h"

namespace DB {
  /**
   * BTrie
   *
   * Implementation of the Nested BTrie Index (specialised to always hold
   * records with two columns). In this implementation, all nodes perform
   * redistribution after deletions, but only leaf nodes perform redistributions
   * after insertions.
   */
  struct BTrie {
    /**
     * Diff
     *
     * Returned by updates to signal what (if any) changes need to be made to a
     * parent node to reflect the updates made to a child node.
     */
    struct Diff {
      Propagate prop;
      int key;

      union {
        page_id pid;
        Siblings sib;
      };
    };

    /**
     * BTrie::leaf
     *
     * Create a new leaf node with the given stride.
     *
     * @param stride The width of each individual record in the BTrie
     * @return The page ID of the new leaf.
     */
    static page_id leaf(int stride);

    /**
     * BTrie::branch
     *
     * Create a new branch node with two children.
     *
     * @param left  The page ID of the left branch
     * @param key   The separating key
     * @param right The page ID of the right branch
     * @return A branch node with structure [left | key | right]. Note that
     *         keys(left) <= key < keys(right) should hold for this branch to be
     *         a valid BTrie Node.
     */
    static page_id branch(page_id left, int key, page_id right);

    /**
     * BTrie::onHeap
     *
     * Create a free-standing BTrie node, outside of database managed pages, in
     * just enough memory to hold the required number of slots at the given size.
     *
     * @param stride The size of each slot
     * @param size   The number of slots.
     *
     * @return A pointer to memory allocated on the heap holding the node. It is
     *         returned as a char pointer because that is how it is allocated,
     *         and how it should be freed (using delete[]).
     */
    static char * onHeap(int stride, int size);

    /**
     * BTrie::load
     *
     * Load a page in and cast it as a BTrie.
     *
     * @param nid The Page ID of the node.
     * @return The pointer to the page, as a BTrie.
     */
    static BTrie *load(page_id nid);

    /**
     * BTrie::reserve
     *
     * Look for a key in the leaves of this tree. If there is no slot for this
     * key, one is created. The page_id of the leaf and the index of the slot in
     * it are output by assignment to the provided references. The return value
     * is used to propagate insertions.
     *
     * @param nid  The page id of the node to look in.
     *
     * @param key  The key to search for.
     *
     * @param sibs Mask representing which neighbours of this node are
     *             siblings. I.e. If both neighbours are siblings, it will have
     *             a value of `LEFT_SIB | RIGHT_SIB`.
     *
     * @param &pid Set to the page_id that is set to the page_id for the leaf.
     *
     * @param &keyPos Set to the position in the node where the slot can be
     *                found.
     *
     * @return An update for the caller. Reserving a slot for the key may cause
     *         a node to be split, or redistributed, in which case the caller
     *         must update its records to reflect that.
     */
    static Diff reserve(page_id nid, int key, Siblings sibs,
                        page_id &pid, int &keyPos);

    /**
     * BTrie::deleteIf
     *
     * Remove a key from the tree, if it exists in the key and satisfies the
     * predicate. The return value is used to propagate the deletion upwards.
     *
     * @param nid  The page id of the node to look in.
     *
     * @param key  The key to delete.
     *
     * @param family Information about the node's siblings in its parent node.
     *
     * @param predicate Function used to determine whether the key should be
     * deleted.
     *
     * @return An update for the caller. Deleting a slot may cause the node to
     *         be merged or redistributed, which should be reflected in its
     *         parent.
     */
    static Diff deleteIf(page_id nid, int key,
                         Family family,
                         std::function<bool(page_id, int)> predicate);

    /**
     * BTrie::find
     *
     * Find the leaf node in the tree containing the smallest key greater than
     * the given key.
     *
     * @param nid The page ID of root node of the BTrie to search in.
     * @param key The search key.
     * @param &foundPID The reference that will be set to the page_id of the
     *                  leaf.
     * @param &foundPos The reference that will be set to the position in the
     *                  leaf to find the key at.
     */
    static void find(page_id nid, int key, page_id &foundPID, int &foundPos);

    /**
     * BTrie::findKey
     *
     * @param key The key to search for.
     * @return The index of the slot in the node corresponding to the smallest
     *         key greater than or equal to the one provided.
     */
    int findKey(int key);

    /**
     * BTrie::split
     *
     * Split the data in this node across two nodes.
     *
     * @param pid    The page ID corresponding to this node
     * @param &pivot A reference that will be filled with the index at which
     *               the node was split.
     *
     * @return A struct containing the the key in the middle of the split, and
     *         the ID of the new page.
     */
    Diff split(page_id pid, int &pivot);

    /**
     * BTrie::merge
     *
     * Append the data from the other node, onto the end of this node's data
     * section. Pre conditions include: The types of this node and that node
     * must match, the two nodes must be neighbouring, and this node must have
     * enough space for the data from both nodes.
     *
     * @param nid  The page_id of this node.
     *
     * @param that The node to merge into this one.
     *
     * @param part The key that partitions these two nodes (When merging
     *             branches this needs to be added back in).
     */
    void merge(page_id nid, BTrie *that, int part);

    /**
     * BTrie::getType
     *
     * @return The type of the node.
     */
    NodeType getType() const;

    /**
     * BTrie::getCount
     *
     * @return The number of children in this BTrie node.
     */
    int getCount() const;

    /**
     * BTrie::getPrev
     *
     * @return The Page ID of the node previous to this one.
     */
    page_id getPrev() const;

    /**
     * BTrie::getNext
     *
     * @return The Page ID of the node after this one.
     */
    page_id getNext() const;

    /**
     * BTrie::isEmpty
     *
     * Check if the node has no slots filled.
     */
    bool isEmpty() const;

    /**
     * BTrie::isFull
     *
     * Check if the node is full (all slots filled)
     */
    bool isFull() const;

    /**
     * BTrie::isUnderOccupied
     *
     * Check if more than half the nodes are empty.
     */
    bool isUnderOccupied() const;

    /**
     * BTrie::slot
     *
     * Get the pointer to the slot at a given index.
     *
     * @param index The slot index.
     */
    int *slot(int index);

  private:

    static const int BRANCH_STRIDE;
    static const int BRANCH_SPACE;
    static const int LEAF_SPACE;

    /**
     * BTrie::BTrie
     *
     * Default constructor. Made private so that BTrie's static functions must
     * be used to create instances.
     */
    BTrie() = default;

    NodeType type;

    int count;
    page_id prev, next;

    union {
      struct {
        int data[1];
      } b;

      struct {
        int stride;
        int data[1];
      } l;
    };

    /**
     * (private) BTrie::makeRoom
     *
     * Make a space of appropriate size at the given index to fit an extra
     * slot. This function does not make sure there is enough room.
     *
     * @param index The position at which to make room.
     * @param size  Number of slots to make room for (defaults to 1).
     */
    void makeRoom(int index, int size = 1);
  };
}

#endif // DB_BTRIE_H
