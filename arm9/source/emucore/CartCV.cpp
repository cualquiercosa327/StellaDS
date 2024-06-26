//============================================================================
//
//   SSSS    tt          lll  lll
//  SS  SS   tt           ll   ll
//  SS     tttttt  eeee   ll   ll   aaaa
//   SSSS    tt   ee  ee  ll   ll      aa
//      SS   tt   eeeeee  ll   ll   aaaaa  --  "An Atari 2600 VCS Emulator"
//  SS  SS   tt   ee      ll   ll  aa  aa
//   SSSS     ttt  eeeee llll llll  aaaaa
//
// Copyright (c) 1995-2022 by Bradford W. Mott, Stephen Anthony
// and the Stella Team
//
// This file has been modified by Dave Bernazzani (wavemotion-dave)
// for optimized execution on the DS/DSi platform. Please seek the
// official Stella source distribution which is far cleaner, newer,
// and better maintained.
//
// See the file "License.txt" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//============================================================================
#include <assert.h>
#include "CartCV.hxx"
#include "Random.hxx"
#include "System.hxx"
#include <iostream>

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CartridgeCV::CartridgeCV(const uInt8* image, uInt32 size)
{
  uInt32 addr;
  if(size == 2048)
  {
    // Copy the ROM image into my buffer
    for(uInt32 addr = 0; addr < 2048; ++addr)
    {
      myImage[addr] = image[addr];
    }

    // Initialize RAM with random values
    Random random;
    for(uInt32 i = 0; i < 1024; ++i)
    {
      myRAM[i] = random.next();
    }
  }
  else if(size == 4096)
  {
    // The game has something saved in the RAM
    // Usefull for MagiCard program listings

    // Copy the ROM image into my buffer
    for(addr = 0; addr < 2048; ++addr)
    {
      myImage[addr] = image[addr + 2048];
    }

    // Copy the RAM image into my buffer
    for(addr = 0; addr < 1024; ++addr)
    {
      myRAM[addr] = image[addr];
    }

  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CartridgeCV::~CartridgeCV()
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const char* CartridgeCV::name() const
{
  return "CV";
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeCV::reset()
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeCV::install(System& system)
{
  mySystem = &system;
  uInt16 shift = mySystem->pageShift();
  uInt16 mask = mySystem->pageMask();

  // Make sure the system we're being installed in has a page size that'll work
  assert((0x1800 & mask) == 0);

  page_access.directPokeBase = 0;
  page_access.device = this;

  // Map ROM image into the system
  for(uInt32 address = 0x1800; address < 0x2000; address += (1 << shift))
  {
    page_access.directPeekBase = &myImage[address & 0x07FF];
    mySystem->setPageAccess(address >> mySystem->pageShift(), page_access);
  }

  // Set the page accessing method for the RAM writing pages
  for(uInt32 j = 0x1400; j < 0x1800; j += (1 << shift))
  {
    page_access.device = this;
    page_access.directPeekBase = 0;
    page_access.directPokeBase = &myRAM[j & 0x03FF];
    mySystem->setPageAccess(j >> shift, page_access);
  }

  // Set the page accessing method for the RAM reading pages
  for(uInt32 k = 0x1000; k < 0x1400; k += (1 << shift))
  {
    page_access.device = this;
    page_access.directPeekBase = &myRAM[k & 0x03FF];
    page_access.directPokeBase = 0;
    mySystem->setPageAccess(k >> shift, page_access);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt8 CartridgeCV::peek(uInt16 address)
{
  return myImage[address & 0x07FF];
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeCV::poke(uInt16, uInt8)
{
  // This is ROM so poking has no effect :-)
}
