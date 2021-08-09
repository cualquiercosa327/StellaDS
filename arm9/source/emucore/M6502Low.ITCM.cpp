//============================================================================
//
// MM     MM  6666  555555  0000   2222
// MMMM MMMM 66  66 55     00  00 22  22
// MM MMM MM 66     55     00  00     22
// MM  M  MM 66666  55555  00  00  22222  --  "A 6502 Microprocessor Emulator"
// MM     MM 66  66     55 00  00 22
// MM     MM 66  66 55  55 00  00 22
// MM     MM  6666   5555   0000  222222
//
// Copyright (c) 1995-1998 by Bradford W. Mott
//
// See the file "license" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//
// $Id: M6502Low.cxx,v 1.2 2002/05/13 19:10:25 stephena Exp $
//============================================================================
#include "Cart.hxx"
#include "CartAR.hxx"
#include "M6502Low.hxx"
#include "TIA.hxx"

uInt16 PC   __attribute__ ((aligned (4))) __attribute__((section(".dtcm")));   // Program Counter
uInt8 A     __attribute__((section(".dtcm")));   // Accumulator
uInt8 X     __attribute__((section(".dtcm")));   // X index register
uInt8 Y     __attribute__((section(".dtcm")));   // Y index register
uInt8 SP    __attribute__((section(".dtcm")));   // Stack Pointer

uInt8 N     __attribute__((section(".dtcm")));   // N flag for processor status register
uInt8 V     __attribute__((section(".dtcm")));   // V flag for processor status register
uInt8 B     __attribute__((section(".dtcm")));   // B flag for processor status register
uInt8 D     __attribute__((section(".dtcm")));   // D flag for processor status register
uInt8 I     __attribute__((section(".dtcm")));   // I flag for processor status register
uInt8 notZ  __attribute__((section(".dtcm")));   // Z flag complement for processor status register
uInt8 C     __attribute__((section(".dtcm")));   // C flag for processor status register

uInt32 NumberOfDistinctAccesses __attribute__((section(".dtcm")));
uInt8 noBanking __attribute__((section(".dtcm"))) = 0;

extern CartridgeAR *myAR;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
M6502Low::M6502Low(uInt32 systemCyclesPerProcessorCycle)
    : M6502(systemCyclesPerProcessorCycle)
{
    NumberOfDistinctAccesses = 0;
    //asm(".rept 2 ; nop ; .endr");
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
M6502Low::~M6502Low()
{
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#define fake_peek()  gSystemCycles++;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline uInt8 M6502Low::peek(uInt16 address)
{
  gSystemCycles++;

  PageAccess& access = myPageAccessTable[(address & MY_ADDR_MASK) >> MY_PAGE_SHIFT];
  if(access.directPeekBase != 0) myDataBusState = *(access.directPeekBase + (address & MY_PAGE_MASK));
  else myDataBusState = access.device->peek(address);

  return myDataBusState;    
}

inline uInt8 M6502Low::peek_PC(uInt16 address)
{
  gSystemCycles++;

  PageAccess& access = myPageAccessTable[(address & MY_ADDR_MASK) >> MY_PAGE_SHIFT];
  if(access.directPeekBase != 0) myDataBusState = *(access.directPeekBase + (address & MY_PAGE_MASK));
  else myDataBusState = access.device->peek(address);

  return myDataBusState;    
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline void M6502Low::poke(uInt16 address, uInt8 value)
{
  gSystemCycles++;
    
  // TIA access is common... filter that one out first...
  if (address < 0x80)
  {
      extern TIA *theTIA;
      theTIA->poke(address, value);
  }
  else
  {
      PageAccess& access = myPageAccessTable[(address & MY_ADDR_MASK) >> MY_PAGE_SHIFT];
      if(access.directPokeBase != 0) *(access.directPokeBase + (address & MY_PAGE_MASK)) = value;
      else access.device->poke(address, value);
  }
    
  myDataBusState = value;    
}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool M6502Low::execute(uInt16 number)
{
  // ----------------------------------------------------------------
  // For Starpath Supercharger games, we must track distinct memory
  // access. This takes time so we don't do it for other game types...
  // ----------------------------------------------------------------
  if (myCartInfo.special == SPEC_AR)
  {
    return execute_AR(number);   
  }
  else if (noBanking)
  {
      return execute_NB(number);
  }
      
  uInt16 fast_loop = number;
  // Clear all of the execution status bits except for the fatal error bit
  myExecutionStatus &= FatalErrorBit;

  // Loop until execution is stopped or a fatal error occurs
  for(;;)
  {
    uInt16 operandAddress=0;
    uInt8 operand=0;
    for(; !myExecutionStatus && (fast_loop != 0); --fast_loop)
    {
      // Get the next 6502 instruction - do this the fast way!
      gSystemCycles++;

      PageAccess& access = myPageAccessTable[(PC & MY_ADDR_MASK) >> MY_PAGE_SHIFT];
      if(access.directPeekBase != 0) myDataBusState = *(access.directPeekBase + (PC & MY_PAGE_MASK));
      else myDataBusState = access.device->peek(PC);        
      PC++;

      // 6502 instruction emulation is generated by an M4 macro file
      switch (myDataBusState)
      {
        #include "M6502Low.ins"        
      }
    }
      
    if (myExecutionStatus)
    {
        // See if we need to handle an interrupt
        if(myExecutionStatus & (MaskableInterruptBit | NonmaskableInterruptBit))
        {
          // Yes, so handle the interrupt
          interruptHandler();
        }

        // See if execution has been stopped
        if(myExecutionStatus & StopExecutionBit)
        {
          // Yes, so answer that everything finished fine
          return true;
        }
        else
        // See if a fatal error has occured
        if(myExecutionStatus & FatalErrorBit)
        {
          // Yes, so answer that something when wrong
          return false;
        }
    }
    else return true;  // we've executed the specified number of instructions
  }
}



// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline uInt8 M6502Low::peek_NB(uInt16 address)
{
  gSystemCycles++;

  if (address < 0x1000)
  {
      PageAccess& access = myPageAccessTable[(address & MY_ADDR_MASK) >> MY_PAGE_SHIFT];
      if(access.directPeekBase != 0) myDataBusState = *(access.directPeekBase + (address & MY_PAGE_MASK));
      else myDataBusState = access.device->peek(address);
  }
  else
  {
    myDataBusState = fast_cart_buffer[address&0xFFF];    
  }

  return myDataBusState;    
}

inline uInt8 M6502Low::peek_PCNB(uInt16 address)
{
  gSystemCycles++;
  myDataBusState = fast_cart_buffer[address&0xFFF];
  return myDataBusState;    
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void M6502Low::interruptHandler()
{
  // Handle the interrupt
  if((myExecutionStatus & MaskableInterruptBit) && !I)
  {
    gSystemCycles += 7; // 7 cycle operation
    mySystem->poke(0x0100 + SP--, (PC - 1) >> 8);		// The high byte of the return address
    mySystem->poke(0x0100 + SP--, (PC - 1) & 0x00ff);	// The low byte of the return address
    mySystem->poke(0x0100 + SP--, PS() & (~0x10));	// The status byte from the processor status register
    D = false;	// Set our flags
    I = true;
    PC = (uInt16)mySystem->peek(0xFFFE) | ((uInt16)mySystem->peek(0xFFFF) << 8);	// Grab the address from the interrupt vector
  }
  else if(myExecutionStatus & NonmaskableInterruptBit)
  {
    gSystemCycles += 7; // 7 cycle operation
    mySystem->poke(0x0100 + SP--, (PC - 1) >> 8);
    mySystem->poke(0x0100 + SP--, (PC - 1) & 0x00ff);
    mySystem->poke(0x0100 + SP--, PS() & (~0x10));
    D = false;
    PC = (uInt16)mySystem->peek(0xFFFA) | ((uInt16)mySystem->peek(0xFFFB) << 8);
  }

  // Clear the interrupt bits in myExecutionStatus
  myExecutionStatus &= ~(MaskableInterruptBit | NonmaskableInterruptBit);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const char* M6502Low::name() const
{
  return "M6502Low";
}


// ==============================================================================
// Special AR Cart Handling below... this requries us to track distinct
// memory fetches (so we can emulate the 5-fetch write cycle of the Starpath
// Supercharger) as well as more complicated bank switching / RAM loading...
// This uses significant DS memory space but we have plenty of RAM and the 
// Starpath Supercharger "AR" games are some of the better games on the system 
// so we'll go the extra mile here...
// ==============================================================================

#define DISTINCT_THRESHOLD  5
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline uInt8 M6502Low::peek_AR(uInt16 address)
{
  NumberOfDistinctAccesses++;
  gSystemCycles++;
    
  if (address & 0xF000)
  {
      uInt16 addr = address & 0x0FFF;     // Map down to 4k...
      if (addr & 0x0800)  // If we are in the upper bank...
      {
          // Is the "dummy" SC BIOS hotspot for reading a load being accessed?
          if(addr == 0x0850 && bPossibleLoad)
          {
            // Get load that's being accessed (BIOS places load number at 0x80)
            uInt8 load = mySystem->peek(0x0080);

            // Read the specified load into RAM
            myAR->loadIntoRAM(load);

            return myImage1[addr];
          }
          // Is the bank configuration hotspot being accessed?
          else if(addr == 0x0FF8)
          {
            // Yes, so handle bank configuration
            myWritePending = false;
            myAR->bankConfiguration(myDataHoldRegister);
          }
          // Handle poke if writing enabled
          else if (myWritePending)
          {
              if (NumberOfDistinctAccesses >= DISTINCT_THRESHOLD)
              {
                  if(!bPossibleLoad)    // Can't poke to ROM :-)
                     myImage1[addr] = myDataHoldRegister;
                  myWritePending = false;
              }
          }

          return myImage1[addr];
      }
      else // WE are in the lower bank
      {
          // Is the data hold register being set?
          if((!(addr & 0x0F00)) && (!myWritePending))
          {
            myDataHoldRegister = addr;
            NumberOfDistinctAccesses = 0;
            if (myWriteEnabled) myWritePending = true;
          }
          // Handle poke if writing enabled
          else if (myWritePending)
          {
              if (NumberOfDistinctAccesses >= DISTINCT_THRESHOLD)
              {
                  myImage0[addr] = myDataHoldRegister;
                  myWritePending = false;
              }
          }

         return myImage0[addr];
      }
  }
  else
  {
      PageAccess& access = myPageAccessTable[(address & MY_ADDR_MASK) >> MY_PAGE_SHIFT];
      if(access.directPeekBase != 0) myDataBusState = *(access.directPeekBase + (address & MY_PAGE_MASK));
      else myDataBusState = access.device->peek(address);        
      return myDataBusState; 
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline void M6502Low::poke_AR(uInt16 address, uInt8 value)
{
  NumberOfDistinctAccesses++;
  gSystemCycles++;  
    
  if (address & 0xF000)
  {
      uInt16 addr = address & 0x0FFF; // Map down to 4k...

      // Is the data hold register being set?
      if(!(addr & 0x0F00) && (!myWritePending))
      {
        myDataHoldRegister = addr;
        NumberOfDistinctAccesses = 0;
        if (myWriteEnabled) myWritePending = true;
      }
      // Is the bank configuration hotspot being accessed?
      else if (addr == 0x0FF8)
      {
        // Yes, so handle bank configuration
        myWritePending = false;
        myAR->bankConfiguration(myDataHoldRegister);
      }
      // Handle poke if writing enabled
      else if (myWritePending)
      {
          if (NumberOfDistinctAccesses >= DISTINCT_THRESHOLD)
          {
            if((addr & 0x0800) == 0)
              myImage0[addr] = myDataHoldRegister;
            else if(!bPossibleLoad)    // Can't poke to ROM :-)
              myImage1[addr] = myDataHoldRegister;
            myWritePending = false;
          }
      }
  }
  else
  {
      PageAccess& access = myPageAccessTable[(address & MY_ADDR_MASK) >> MY_PAGE_SHIFT];
      if(access.directPokeBase != 0) *(access.directPokeBase + (address & MY_PAGE_MASK)) = value;
      else access.device->poke(address, value);
      myDataBusState = value;
  }
}


bool M6502Low::execute_NB(uInt16 number)
{
  uInt16 fast_loop = number;
  // Clear all of the execution status bits except for the fatal error bit
  myExecutionStatus &= FatalErrorBit;

  // Loop until execution is stopped or a fatal error occurs
  for(;;)
  {
    uInt16 operandAddress=0;
    uInt8 operand=0;
    for(; !myExecutionStatus && (fast_loop != 0); --fast_loop)
    {
      // Get the next 6502 instruction - do this the fast way!
      gSystemCycles++;
      myDataBusState = fast_cart_buffer[PC & 0xFFF];
      PC++;

      // 6502 instruction emulation is generated by an M4 macro file
      switch (myDataBusState)
      {
        // A trick of the light... here we map peek/poke to the "AR" cart versions. Slower but needed for some games...
        #define peek    peek_NB
        #define peek_PC peek_PCNB              
        #include "M6502Low.ins"        
      }
    }
      
    if (myExecutionStatus)
    {
        // See if we need to handle an interrupt
        if(myExecutionStatus & (MaskableInterruptBit | NonmaskableInterruptBit))
        {
          // Yes, so handle the interrupt
          interruptHandler();
        }

        // See if execution has been stopped
        if(myExecutionStatus & StopExecutionBit)
        {
          // Yes, so answer that everything finished fine
          return true;
        }
        else
        // See if a fatal error has occured
        if(myExecutionStatus & FatalErrorBit)
        {
          // Yes, so answer that something when wrong
          return false;
        }
    }
    else return true;  // we've executed the specified number of instructions
  }
}


bool M6502Low::execute_AR(uInt16 number)
{
  uInt16 fast_loop = number;
    
  // Clear all of the execution status bits except for the fatal error bit
  myExecutionStatus &= FatalErrorBit;

  // Loop until execution is stopped or a fatal error occurs
  for(;;)
  {
    uInt16 operandAddress;
    uInt8 operand;
      
    for(; !myExecutionStatus && (fast_loop != 0); --fast_loop)
    {
      // Get the next 6502 instruction
      myDataBusState = peek_AR(PC++);
        
      // 6502 instruction emulation is generated by an M4 macro file
      switch (myDataBusState)
      {
        // A trick of the light... here we map peek/poke to the "AR" cart versions. Slower but needed for some games...
        #undef peek
        #undef peek_PC
        #define peek    peek_AR
        #define peek_PC peek_AR              
        #define poke    poke_AR
        #include "M6502Low.ins"        
      }
    }

    if (myExecutionStatus)
    {
        // See if we need to handle an interrupt
        if(myExecutionStatus & (MaskableInterruptBit | NonmaskableInterruptBit))
        {
          // Yes, so handle the interrupt
          interruptHandler();
        }

        // See if execution has been stopped
        if(myExecutionStatus & StopExecutionBit)
        {
          // Yes, so answer that everything finished fine
          return true;
        }
        else
        // See if a fatal error has occured
        if(myExecutionStatus & FatalErrorBit)
        {
          // Yes, so answer that something when wrong
          return false;
        }
    } 
    else return true;  // we've executed the specified number of instructions
  }
}
