#ifndef DB_TRIE_H
#define DB_TRIE_H

namespace DB {
  /**
   * A collection of enums and structs that are shared across both nested B+
   * tries and nested Fractal tries.
   */

  /**
   * NodeType
   *
   * Enum to tag BTrie Nodes with their type (Leaf or Branch)
   */
  enum NodeType { Branch, Leaf };

  /**
   * Siblings
   *
   * Enum used to tell a child node which of its neighbours are siblings. This
   * information is used when redistributing.
   */
  enum Siblings : unsigned int {
    NO_SIBS   = 0,
    LEFT_SIB  = 1 << 1,
    RIGHT_SIB = 1 << 2
  };

  /**
   * Family
   *
   * Information about sibling nodes, used for redistributions.
   */
  struct Family {
    Siblings sibs;
    int leftKey;
    int rightKey;
  };

  /**
   * Propagate
   *
   * Tag designating how the child nodes were modified during insertions and
   * deletions, so that the parent node may be updated appropriately.
   */
  enum Propagate {
    PROP_NOTHING,
    PROP_CHANGE,
    PROP_SPLIT,
    PROP_MERGE,
    PROP_REDISTRIB,
  };

  /**
   * Operators for combining Sibling masks whilst preserving type-safety.
   */
  Siblings operator|(Siblings, Siblings);
  Siblings operator&(Siblings, Siblings);

  Siblings &operator|=(Siblings &, Siblings);
}

#endif // DB_TRIE_H
