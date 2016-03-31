#include <iostream>
#include <memory>
#include <vector>

#include "allocator.h"
#include "bufmgr.h"
#include "db.h"
#include "dim.h"
#include "heap_file.h"
#include "leapfrog_triejoin.h"
#include "table.h"
#include "trie_iterator.h"

using namespace std;

void scanByLine(std::unique_ptr<DB::TrieIterator> &it, int depth, int *rec, int stride, int &count);

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

    DB::Table R(0, 1);
    R.loadFromFile("data/R1.txt");

    auto it = R.scan();

    int count = 0; int *buf = new int[2];
    scanByLine(it, 0, buf, 2, count);
    cout << "\n#R: " << count << endl;
    delete[] buf;

  } catch(exception &e) {
    cerr << "\n\nIncDB terminated due to exception: "
         << e.what() << endl;
  }
  return 0;
}

void
scanByLine(std::unique_ptr<DB::TrieIterator> &it,
           int depth, int *rec, int stride, int &count)
{
  if (depth >= stride) {
    for (int i = 0; i < stride; ++i)
      cout << rec[i] << " ";
    cout << endl;
    count++;
    return;
  }

  it->open();
  while (!it->atEnd()) {
    rec[depth] = it->key();
    scanByLine(it, depth + 1, rec, stride, count);
    it->next();
  }
  it->up();
}
