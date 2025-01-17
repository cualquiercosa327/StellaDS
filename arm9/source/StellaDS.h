#ifndef __DS_TOOLS_H
#define __DS_TOOLS_H

#include <nds.h>

#include "Console.hxx"

#define WAITVBL swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank();

#define STELLADS_MENUINIT 0x01
#define STELLADS_MENUSHOW 0x02
#define STELLADS_PLAYINIT 0x03 
#define STELLADS_PLAYGAME 0x04 
#define STELLADS_QUITSTDS 0x05

typedef enum {
  EMUARM7_INIT_SND = 0x123C,
  EMUARM7_STOP_SND = 0x123D,
  EMUARM7_PLAY_SND = 0x123E,
} FifoMesType;

#define MAX_ROMS_PER_DIRECTORY  1500
#define MAX_FILE_NAME_LEN       199

typedef struct FICtoLoad {
  char  filename[MAX_FILE_NAME_LEN];
  uInt8 directory;
} FICA2600;

extern Console* theConsole;
extern Sound* theSound;

extern FICA2600 vcsromlist[];

extern uInt16 atari_frames;
extern uInt8  bInitialDiffSet;
extern uInt8  tv_type_requested;
extern uInt8  gSaveKeyEEWritten;
extern uInt8  gSaveKeyIsDirty;
extern uInt16 mySoundFreq;
extern uInt16 emuState;
extern uint8  sound_buffer[SOUND_SIZE];
extern uint8  bHaltEmulation; 
extern uint8  bScreenRefresh;
extern uInt32 gAtariFrames;
extern uInt32 gTotalAtariFrames;
extern uInt16 console_color;

#define ds_GetTicks() (TIMER0_DATA)

extern void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait);

extern void dsInitScreenMain(void);
extern void dsInitTimer(void);
extern void dsInitPalette(void);

extern void dsShowScreenEmu(void);
extern void dsShowScreenMain(bool bFull);
extern void dsFreeEmu(void);
extern bool dsLoadGame(char *filename);

extern bool dsWaitOnQuit(void);
extern unsigned int dsWaitForRom(void);
extern unsigned int dsWaitOnMenu(unsigned int actState);

extern void dsPrintValue(int x, int y, unsigned int isSelect, char *pchStr);

extern void dsMainLoop(void);

extern void dsInstallSoundEmuFIFO(void);

extern void vcsFindFiles(void);


#endif
