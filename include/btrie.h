#ifndef DB_BTRIE_H
#define DB_BTRIE_H

#include <cstddef>
#include <cstring>
#include <stdexcept>

#include "allocator.h"
#include "db.h"

namespace DB {
  /**
   * BTrie
   *
   * Implementation of the Nested BTrie Index (specialised to always hold
   * records with two columns).
   */
  struct BTrie {

    /**
     * SplitInfo
     *
     * Returned by insertions to signal whether the insertion caused a split,
     * and if so, what should be put in the parent node.
     */
    struct SplitInfo {
      int key;
      page_id pid;

      inline bool
      isSplit()
      {
        return pid != INVALID_PAGE;
      }

      inline bool
      isNoChange()
      {
        return pid == INVALID_PAGE && key < 0;
      }
    };

    static constexpr SplitInfo NO_SPLIT  = {.key =  0, .pid = INVALID_PAGE};
    static constexpr SplitInfo NO_CHANGE = {.key = -1, .pid = INVALID_PAGE};

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
     * BTrie::load
     *
     * Load a page in and cast it as a BTrie.
     *
     * @param nid The Page ID of the node.
     * @return The pointer to the page, as a BTrie.
     */
    static BTrie *load(page_id nid);

    /**
     * BTrie::lookup
     *
     * Look for a key in the node, and set the page_id and offset in the page
     * at which to find said slot. If the slot does not exist, one is created
     * for the key. The return value is used to propagate insertions
     *
     * @param nid  The page id of the node to look in.
     *
     * @param key  The key to search for.
     *
     * @param &pid A reference to a page_id that is set to the page_id for the
     *             leaf. This page is pinned by the routine, so the caller
     *             must unpin it once finished.
     *
     * @param &keyPos A reference to the position in the node where the slot can be
     *                found.
     *
     * @return An update for the caller. The lookup may cause a node to be
     *         split, in which case the caller must update its records to
     *         reflect that. If no split occurred, then the split will have an
     *         invalid page id. Furthermore, if no updates were made at all,
     *         the key will be negative.
     */
    static SplitInfo lookup(page_id nid, int key, page_id &pid, int &keyPos);

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
    SplitInfo split(page_id pid, int &pivot);

    /**
     * BTrie::isFull
     *
     * Check if the node is full (all slots filled)
     */
    bool isFull() const;

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

    enum { Branch, Leaf } type;

    int count;

    union {
      struct {
        int data[1];
      } b;

      struct {
        page_id prev, next;
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
     */
    void makeRoom(int index);

  };
}

#endif // DB_BTRIE_H
