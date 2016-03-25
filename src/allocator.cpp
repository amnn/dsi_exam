#include "allocator.h"

#include <cstdlib>
#include <exception>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <unistd.h>

namespace DB {
  Allocator::Allocator(const char * fname,
                       unsigned     psize,
                       unsigned     pcount)
    : mPageSize ( psize )
    , mNumPages ( pcount )
    , mSpaceMap ( psize * pcount, false )
    , mName     ( fname )
  {
    // Open the file, and test for success.
    mFD = open(fname, O_RDWR | O_CREAT | O_EXCL, 0666);
    if (mFD < 0) {
      std::string err("Could not create database file: ");
      err += fname; err += "!";
      throw std::runtime_error(err);
    }

    // Set the length of the database file.
    if (ftruncate(mFD, psize * pcount) < 0)
      throw std::runtime_error("Could not resize database file.\n");
  }

  Allocator::~Allocator()
  {
    close(mFD);
    unlink(mName.c_str());
    mFD = -1;
  }

  page_id
  Allocator::palloc(int num)
  {
    return 0;
  }

  void
  read(page_id pid, char *buf)
  {
  }

  void
  write(page_id pid, char *buf)
  {
  }
}
