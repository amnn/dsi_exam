#include "trie.h"

#include <type_traits>

namespace DB {
  Siblings
  operator|(Siblings s, Siblings t)
  {
    using UL = std::underlying_type<Siblings>::type;
    return Siblings(static_cast<UL>(s) | static_cast<UL>(t));
  }

  Siblings
  operator&(Siblings s, Siblings t)
  {
    using UL = std::underlying_type<Siblings>::type;
    return Siblings(static_cast<UL>(s) & static_cast<UL>(t));
  }

  Siblings &
  operator|=(Siblings &s, Siblings t)
  {
    s = s | t;
    return s;
  }
}
