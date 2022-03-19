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
// Copyright (c) 1995-2005 by Bradford W. Mott
//
// See the file "license" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//
//============================================================================

#include <assert.h>
#include <iostream>
#include "CartEF.hxx"
#include "Random.hxx"
#include "System.hxx"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CartridgeEF::CartridgeEF(const uInt8* image)
{
  // Copy the ROM image into my buffer
  for(uInt32 addr = 0; addr < (64*1024); ++addr)
  {
    myImage[addr] = image[addr];
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CartridgeEF::~CartridgeEF()
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const char* CartridgeEF::name() const
{
  return "CartridgeEF";
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeEF::reset()
{
  // Upon reset we switch to bank 1
  bank(1);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeEF::install(System& system)
{
  mySystem = &system;
  uInt16 shift = mySystem->pageShift();
  uInt16 mask = mySystem->pageMask();

  // Make sure the system we're being installed in has a page size that'll work
  assert((0x1000 & mask) == 0);

  page_access.directPeekBase = 0;
  page_access.directPokeBase = 0;
  page_access.device = this;
  
  // Set the page accessing methods for the hot spots
  for(uInt32 i = (0x1FE0 & ~mask); i < 0x2000; i += (1 << shift))
  {
    mySystem->setPageAccess(i >> shift, page_access);
  }

  // Install pages for bank 1
  bank(1);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt8 CartridgeEF::peek(uInt16 address)
{
  address = address & 0x0FFF;

  // Switch banks if necessary
  if((address >= 0x0FE0) && (address <= 0x0FEF))
  {
    bank(address - 0x0FE0);
  }

  return myImage[myCurrentOffset + address];
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeEF::poke(uInt16 address, uInt8)
{
  address = address & 0x0FFF;

  // Switch banks if necessary
  if((address >= 0x0FE0) && (address <= 0x0FEF))
  {
    bank(address - 0x0FE0);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeEF::bank(uInt16 bank)
{ 
  // Remember what bank we're in
  myCurrentOffset = bank * 4096;

  // Setup the page access methods for the current bank
  uInt32 access_num = 0x1000 >> MY_PAGE_SHIFT;

  // Map ROM image into the system
  for(uInt32 address = 0x0000; address < (0x0FE0U & ~MY_PAGE_MASK); address += (1 << MY_PAGE_SHIFT))
  {
      page_access.directPeekBase = &myImage[myCurrentOffset + address];
      mySystem->setPageAccess(access_num++, page_access);
  }
}
