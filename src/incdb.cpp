#include <iostream>
#include <memory>
#include <vector>

#include "allocator.h"
#include "bufmgr.h"
#include "db.h"
#include "leapfrog_triejoin.h"
#include "table.h"
#include "trie_iterator.h"
using namespace std;

void insert(DB::Table &t, int from, int to, int step, bool isRepeat);
void remove(DB::Table &t, int from, int to, int step, bool isRepeat);
void scan(std::unique_ptr<DB::TrieIterator> it, int step);
void scanAll(std::unique_ptr<DB::TrieIterator> &it, int depth);

int
main(int, char **)
{
  try {
    DB::Allocator a(DB::Dim::NAME,
                    DB::Dim::PAGE_SIZE,
                    DB::Dim::NUM_PAGES);

    DB::BufMgr    b(DB::Dim::POOL_SIZE);

    DB::Global::ALLOC  = &a;
    DB::Global::BUFMGR = &b;

    DB::Table t1(0, 1);

    /**
    cout << a.spaceMap() << endl;

    // Insertion/Deletion Tests

    insert(t1, 1, 33, 1, false);
    remove(t1, 32, 1, -1, false);
    remove(t1, 1, 33, 1, true);

    insert(t1, 32, 1, -1, false);
    remove(t1, 1, 33, 1, false);
    remove(t1, 1, 33, 1, true);

    insert(t1, 1, 33, 1, false);
    insert(t1, 1, 33, 1, true);

    // Iterator Scanning Tests

    scan(t1.scan(), 1);
    remove(t1, 1, 33, 2, false);
    scan(t1.scan(), 1);
    scan(t1.scan(), 3);

    // Singleton test
    scan(t1.singleton(100, 100), 1);
    */

    DB::Table r(0, 1), s(1, 2), t(0, 2);

    r.insert(7, 4);

    s.insert(4, 0);
    s.insert(4, 1);
    s.insert(4, 2);
    s.insert(4, 3);

    t.insert(7, 0);
    t.insert(7, 1);
    t.insert(7, 2);

    r.insert(8, 4);
    t.insert(8, 3);
    t.insert(8, 4);

    vector<unique_ptr<DB::TrieIterator>> query {};
    query.emplace_back(r.scan());
    query.emplace_back(s.scan());
    query.emplace_back(t.scan());

    auto join = unique_ptr<DB::TrieIterator>(new DB::LeapFrogTrieJoin(3, move(query)));

    scanAll(join, 3);

  } catch(exception &e) {
    cerr << "\n\nIncDB terminated due to exception: "
         << e.what() << endl;
  }
  return 0;
}

void
insert(DB::Table &t, int from, int to, int step, bool isRepeat)
{
  cout << "[+]...";
  for (int i = from; (from < to) == (i < to); i += step)
    if (isRepeat == t.insert(i, 1))
      cout << "!I " << i << endl;

  cout << "Done." << endl;
  cout << DB::Global::ALLOC->spaceMap() << endl;
}

void
remove(DB::Table &t, int from, int to, int step, bool isRepeat)
{
  cout << "[-]...";
  for (int i = from; (from < to) == (i < to); i += step)
    if (isRepeat == t.remove(i, 1))
      cout << "!R " << i << endl;

  cout << "Done." << endl;
  cout << DB::Global::ALLOC->spaceMap() << endl;
}

void
scan(std::unique_ptr<DB::TrieIterator> it, int step)
{
  cout << "[~]...";
  it->open();
  while (!it->atEnd()) {
    int k = it->key();
    cout << k << "(";

    it->open();
    bool first = true;
    while (!it->atEnd()) {
      if (!first) cout << ",";
      cout << it->key();
      it->next();

      first &= false;
    }
    cout << ") " << flush;
    it->up();

    if (step == 1)
      it->next();
    else
      it->seek(k + step);
  }
  cout << "Done." << endl;
}

void
scanAll(std::unique_ptr<DB::TrieIterator> &it, int depth)
{
  if (depth <= 0) return;

  it->open();
  bool first = true;
  while (!it->atEnd()) {
    if (!first) cout << " ";
    cout << it->key();

    if (depth > 1) {
      cout << "(";
      scanAll(it, depth - 1);
      cout << ")" << flush;
    }

    it->next();
    first = false;
  }
  it->up();
}
