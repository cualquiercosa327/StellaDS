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
// Copyright (c) 1995-2012 by Bradford W. Mott, Stephen Anthony
// and the Stella Team
//
// See the file "License.txt" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//
// $Id$
//============================================================================
#include <nds.h>
#include <cassert>
#include <cstring>

typedef unsigned long long uInt64;

#include "System.hxx"
#include "CartDPCPlus.hxx"
#include "Thumbulator.hxx"

uInt16 myStartBank = 5;

#define MEM_3KB  (1024 * 3)
#define MEM_4KB  (1024 * 4)
#define MEM_5KB  (1024 * 5)
#define MEM_24KB (1024 * 24)

Thumbulator *myThumbEmulator = NULL;

// The counter registers for the data fetchers
bool   myFastFetch        __attribute__((section(".dtcm"))) = 0;
uInt8* myDisplayImageDPCP __attribute__((section(".dtcm")));  // Pointer to the 4K display ROM image of the cartridge

// The counter registers for the fractional data fetchers
uInt32 myFractionalCounters[8] __attribute__((section(".dtcm")));

// The fractional increments for the data fetchers
uInt32 myFractionalIncrements[8] __attribute__((section(".dtcm")));

// The top registers for the data fetchers
uInt32 myTops[8] __attribute__((section(".dtcm")));

uInt32 myTopsMinusBottoms[8] __attribute__((section(".dtcm")));

// The bottom registers for the data fetchers
uInt32 myBottoms[8] __attribute__((section(".dtcm")));

extern uInt32 myCounters[8];

uInt8 myDPC[24*1024];

uInt8 *myDPCptr __attribute__((section(".dtcm")));

CartridgeDPCPlus *myCartDPC __attribute__((section(".dtcm")));

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CartridgeDPCPlus::CartridgeDPCPlus(const uInt8* image, uInt32 size)
  : myParameterPointer(0),
    mySystemCycles(0),
    myFractionalClocks(0.0)
{
  // Pointer to the program ROM (24K @ 0 byte offset)
  myProgramImage = (uInt8 *)image + MEM_3KB;
        
  memcpy(myDPC, myProgramImage, MEM_24KB);

  memset(fast_cart_buffer, 0, 8192);

  // Pointer to the display RAM
  myDisplayImageDPCP = fast_cart_buffer + MEM_3KB;

  // Pointer to the Frequency ROM (1K @ 28K offset)
  myFrequencyImage = myDisplayImageDPCP + MEM_4KB;

  // Copy DPC display data to Harmony RAM
  memcpy(myDisplayImageDPCP, myProgramImage + MEM_24KB, MEM_5KB);
      
  // Initialize the DPC data fetcher registers
  for(uInt16 i = 0; i < 8; ++i)
  {
    myTops[i] = myBottoms[i] = myCounters[i] = myFractionalIncrements[i] =  myFractionalCounters[i] = myTopsMinusBottoms[i] = 0;
  }

  // Set waveforms to first waveform entry
  myMusicWaveforms[0] = myMusicWaveforms[1] = myMusicWaveforms[2] = 0;

  // Initialize the DPC's random number generator register (must be non-zero)
  myRandomNumber = 0x2B435044; // "DPC+"

  // DPC+ always starts in bank 5
  myStartBank = 5;
      
  myFastFetch = false;

  // Create Thumbulator ARM emulator
  myThumbEmulator = new Thumbulator((uInt16*)(myProgramImage-0xC00));
  
  myFractionalLowMask = (myCartInfo.special == SPEC_OLDDPCP) ? 0x0F0000 : 0x0F00FF;  
        
  myCartDPC = this;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CartridgeDPCPlus::~CartridgeDPCPlus()
{
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const char* CartridgeDPCPlus::name() const
{
  return "CartridgeDPCPlus";
}    

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeDPCPlus::reset()
{
  // Update cycles to the current system cycles
  mySystemCycles = mySystem->cycles();
  myFractionalClocks = 0.0;

  // Upon reset we switch to the startup bank
  bank(myStartBank);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeDPCPlus::systemCyclesReset()
{
  // Get the current system cycle
  uInt32 cycles = mySystem->cycles();

  // Adjust the cycle counter so that it reflects the new value
  mySystemCycles -= cycles;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeDPCPlus::install(System& system)
{
  mySystem = &system;
  uInt16 shift = mySystem->pageShift();
  uInt16 mask = mySystem->pageMask();

  // Make sure the system we're being installed in has a page size that'll work
  assert(((0x1080 & mask) == 0) && ((0x1100 & mask) == 0));

  // Set the page accessing methods for the hot spots
  PageAccess access;
  access.directPeekBase = 0;
  access.directPokeBase = 0;
  access.device = this;

  // Set page accessing method for the DPC reading & writing pages
  for(uInt32 j = 0x1000; j < 0x2000; j += (1 << shift))
  {
    mySystem->setPageAccess(j >> shift, access);
  }

  // Install pages for the startup bank
  bank(myStartBank);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline void CartridgeDPCPlus::clockRandomNumberGenerator()
{
  // Update random number generator (32-bit LFSR)
  myRandomNumber = ((myRandomNumber & (1<<10)) ? 0x10adab1e: 0x00) ^
                   ((myRandomNumber >> 11) | (myRandomNumber << 21));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline void CartridgeDPCPlus::priorClockRandomNumberGenerator()
{
  // Update random number generator (32-bit LFSR, reversed)
  myRandomNumber = ((myRandomNumber & (1<<31)) ?
    ((0x10adab1e^myRandomNumber) << 11) | ((0x10adab1e^myRandomNumber) >> 21) :
    (myRandomNumber << 11) | (myRandomNumber >> 21));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline void CartridgeDPCPlus::updateMusicModeDataFetchers()
{
  // Calculate the number of cycles since the last update
  Int32 cycles = mySystem->cycles() - mySystemCycles;
  mySystemCycles = mySystem->cycles();

  // Calculate the number of DPC OSC clocks since the last update
  double clocks = ((20000.0 * cycles) / 1193191.66666667) + myFractionalClocks;
  Int32 wholeClocks = (Int32)clocks;
  myFractionalClocks = clocks - (double)wholeClocks;

  if(wholeClocks <= 0)
  {
    return;
  }

  // Let's update counters and flags of the music mode data fetchers
  for(int x = 0; x <= 2; ++x)
  {
    myMusicCounters[x] += myMusicFrequencies[x];
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline void CartridgeDPCPlus::callFunction(uInt8 value)
{
  // myParameter
  switch (value)
  {
    case 0: // Parameter Pointer reset
      myParameterPointer = 0;
      break;
    case 1: // Copy ROM to fetcher
      {
         uInt16 ROMdata = (myParameter[1] << 8) + myParameter[0];
         for(int i = 0; i < myParameter[3]; ++i)
             myDisplayImageDPCP[myCounters[myParameter[2] & 0x7]+i] = myProgramImage[ROMdata+i];
         myParameterPointer = 0;
      }
      break;
    case 2: // Copy value to fetcher
      for(int i = 0; i < myParameter[3]; ++i)
        myDisplayImageDPCP[myCounters[myParameter[2]]+i] = myParameter[0];
      myParameterPointer = 0;
      break;
          
    case 254:
    case 255:
      // Call user written ARM code (most likely be C compiled for ARM)
      myThumbEmulator->run();
      break;
  }
}

ITCM_CODE uInt8 CartridgeDPCPlus::peekFetch(uInt8 address)
{
    uInt8 result = 0;
    
    if (address <= 5)
    {
        switch(address)
        {
          case 0x00:  // RANDOM0NEXT - advance and return byte 0 of random
            clockRandomNumberGenerator();
            result = myRandomNumber & 0xFF;
            break;

          case 0x01:  // RANDOM0PRIOR - return to prior and return byte 0 of random
            priorClockRandomNumberGenerator();
            result = myRandomNumber & 0xFF;
            break;

          case 0x02:  // RANDOM1
            result = (myRandomNumber>>8) & 0xFF;
            break;

          case 0x03:  // RANDOM2
            result = (myRandomNumber>>16) & 0xFF;
            break;

          case 0x04:  // RANDOM3
            result = (myRandomNumber>>24) & 0xFF;
            break;

          case 0x05: // AMPLITUDE
          {
            // Update the music data fetchers (counter & flag)
            updateMusicModeDataFetchers();

            // using myDisplayImageDPCP[] instead of myDPC[] because waveforms
            // can be modified during runtime.
            uInt32 i = myDisplayImageDPCP[(myMusicWaveforms[0] << 5) + (myMusicCounters[0] >> 27)] +
                       myDisplayImageDPCP[(myMusicWaveforms[1] << 5) + (myMusicCounters[1] >> 27)] +
                       myDisplayImageDPCP[(myMusicWaveforms[2] << 5) + (myMusicCounters[2] >> 27)];

            result = uInt8(i);
            break;
          }
        }
    }

    return result;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ITCM_CODE uInt8 CartridgeDPCPlus::peek(uInt16 address)  
{
    address &= 0x0FFF;
    return myDPCptr[address];        // This will only get called twice at the reset of the world... after that the driver in M6502Low.cpp will be used
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeDPCPlus::poke(uInt16 address, uInt8 value)
{
    // Get the index of the data fetcher that's being accessed
    uInt8 index = address & 0x07;
    //uInt8 function = ((address - 0x28) >> 3) & 0x0f;
    uInt8 function = address & 0xF8;

    switch(function)
    {
      // DFxFRACLOW - fractional data pointer low byte
      case 0x28:
        myFractionalCounters[index] =
          (myFractionalCounters[index] & myFractionalLowMask) | (uInt16(value) << 8);
        break;

      // DFxFRACHI - fractional data pointer high byte
      case 0x30:
        myFractionalCounters[index] = ((uInt16(value) & 0x0F) << 16) | (myFractionalCounters[index] & 0x00ffff);
        break;

      //DFxFRACINC - Fractional Increment amount
      case 0x38:
        myFractionalIncrements[index] = value;
        myFractionalCounters[index] = myFractionalCounters[index] & 0x0FFF00;
        break;

      // DFxTOP - set top of window (for reads of DFxDATAW)
      case 0x40:
        myTops[index] = value;
        myTopsMinusBottoms[index] = (myTops[index]-myBottoms[index]) & 0xFF;
        break;

      // DFxBOT - set bottom of window (for reads of DFxDATAW)
      case 0x48:
        myBottoms[index] = value;
        myTopsMinusBottoms[index] = (myTops[index]-myBottoms[index]) & 0xFF;
        break;

      // DFxLOW - data pointer low byte
      case 0x50:
        myCounters[index] = (myCounters[index] & 0x0F00) | value ;
        break;

      // Control registers
      case 0x58:
        switch (index)
        {
          case 0x00:  // FASTFETCH - turns on LDA #<DFxDATA mode of value is 0
            myFastFetch = (value == 0);
            break;

          case 0x01:  // PARAMETER - set parameter used by CALLFUNCTION (not all functions use the parameter)
            if(myParameterPointer < 8)
              myParameter[myParameterPointer++] = value;
            break;

          case 0x02:  // CALLFUNCTION
            callFunction(value);
            break;

          case 0x05:  // WAVEFORM0
          case 0x06:  // WAVEFORM1
          case 0x07:  // WAVEFORM2
            myMusicWaveforms[index - 5] =  value & 0x7f;
            break;
          default:
            break;
        }
        break;

      // DFxPUSH - Push value into data bank
      case 0x60:
         myDisplayImageDPCP[--myCounters[index]] = value;
         break;

      // DFxHI - data pointer high byte
      case 0x68:
      {
        myCounters[index] = ((uInt16(value) & 0x0F) << 8) | (myCounters[index] & 0x00ff);
        break;
      }

      case 0x70:
      {
        switch (index)
        {
          case 0x00:  // RRESET - Random Number Generator Reset
          {
            myRandomNumber = 0x2B435044; // "DPC+"
            break;
          }
          case 0x01:  // RWRITE0 - update byte 0 of random number
          {
            myRandomNumber = (myRandomNumber & 0xFFFFFF00) | value;
            break;
          }
          case 0x02:  // RWRITE1 - update byte 1 of random number
          {
            myRandomNumber = (myRandomNumber & 0xFFFF00FF) | (value<<8);
            break;
          }
          case 0x03:  // RWRITE2 - update byte 2 of random number
          {
            myRandomNumber = (myRandomNumber & 0xFF00FFFF) | (value<<16);
            break;
          }
          case 0x04:  // RWRITE3 - update byte 3 of random number
          {
            myRandomNumber = (myRandomNumber & 0x00FFFFFF) | (value<<24);
            break;
          }
          case 0x05:  // NOTE0
          case 0x06:  // NOTE1
          case 0x07:  // NOTE2
          {
            myMusicFrequencies[index-5] = myFrequencyImage[(value<<2)] +
            (myFrequencyImage[(value<<2)+1]<<8) +
            (myFrequencyImage[(value<<2)+2]<<16) +
            (myFrequencyImage[(value<<2)+3]<<24);
            break;
          }
          default:
            break;
        }
        break;
      }

      // DFxWRITE - write into data bank
      case 0x78:
      {
        myDisplayImageDPCP[myCounters[index]++] = value;
        break;
      }

      default:
        break;
    }
    return;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeDPCPlus::bank(uInt16 bank)
{
    // Remember what bank we're in
  myCurrentOffset = bank << 12;
  myDPCptr = &myDPC[myCurrentOffset];
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const uInt8* CartridgeDPCPlus::getImage(int& size) const
{
  size = mySize;
  return 0;
}
