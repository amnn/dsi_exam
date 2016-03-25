#include "allocator.h"

#include <cstdlib>
#include <exception>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace DB {
  Allocator::Allocator(const char * fname,
                       unsigned     psize,
                       unsigned     pcount)
    : mPageSize ( psize )
    , mSpaceMap ( pcount, false )
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
      throw std::runtime_error("Could not resize database file.");
  }

  Allocator::~Allocator()
  {
    close(mFD);
    unlink(mName.c_str());
    mFD = -1;
  }

  page_id
  Allocator::palloc(unsigned num)
  {
    // find a large enough run of free pages in the space map.
    unsigned pid0 = 0, runLen = 0;
    for (unsigned i = 0; i < mSpaceMap.size() && runLen < num; ++i)
      if (mSpaceMap[i]) {
        runLen = 0;
      } else {
        if (runLen == 0) pid0 = i;
        runLen++;
      }

    if (runLen < num) {
      std::stringstream err;
      err << "Cannot allocate " << num << " contiguous pages!";
      throw std::runtime_error(err.str());
    }

    // set the pages in the run as allocated
    for (unsigned i = pid0; i < pid0 + runLen; ++i)
      mSpaceMap[i] = true;

    // return the first page id.
    return pid0;
  }

  void
  Allocator::pfree(page_id pid)
  {
    if(pid < 0 || pid >= mSpaceMap.size())
      std::runtime_error("Bad page id!");

    mSpaceMap[pid] = false;
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
