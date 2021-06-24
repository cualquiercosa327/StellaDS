#include <nds.h>
#include <stdio.h>
#include <fat.h>
#include <dirent.h>
#include <unistd.h>
#include "ds_tools.h"
#include "highscore.h"
#include "Console.hxx"
#include "Cart.hxx"
#include "bgHighScore.h"
#include "bgBottom.h"

#define MAX_HS_GAMES 1000
#define HS_VERSION   0x0001

#define WAITVBL swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank();

#pragma pack(1)

struct score_t 
{
    char    initials[4];
    char    score[7];
    char    reserved[5];
    uint16  year;
    uint8   month;
    uint8   day;
};

struct highscore_t
{
    char    md5sum[33];
    char    notes[21];
    uint16  options;
    struct score_t scores[10];
};

struct highscore_full_t
{
    uint16 version;
    char   last_initials[4];
    struct highscore_t highscore_table[MAX_HS_GAMES];
    uint32 checksum;
} highscores;


extern int bg0, bg0b,bg1b;


uint32 highscore_checksum(void)
{
    char *ptr = (char *)&highscores;
    uint32 sum = 0;
    
    for (int i=0; i<(int)sizeof(highscores) - 4; i++)
    {
           sum = *ptr++;
    }
    return sum;
}

void highscore_init(void) 
{
    bool create_defaults = 0;
    FILE *fp;
    
    strcpy(highscores.last_initials, "   ");
    
    // --------------------------------------------------------------
    // See if the StellaDS high score file exists... if so, read it!
    // --------------------------------------------------------------
    fp = fopen("/data/StellaDS.hi", "rb");
    if (fp != NULL)
    {
        fread(&highscores, sizeof(highscores), 1, fp);
        fclose(fp);
        
        if (highscores.version != HS_VERSION) create_defaults = 1;
        if (highscore_checksum() != highscores.checksum) create_defaults = 1;
    }
    else
    {
        create_defaults = 1;
    }
    
    if (create_defaults)  // Doesn't exist yet or is invalid... create defaults and save it...
    {
        strcpy(highscores.last_initials, "   ");
        
        for (int i=0; i<MAX_HS_GAMES; i++)
        {
            strcpy(highscores.highscore_table[i].md5sum, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
            strcpy(highscores.highscore_table[i].notes, "                    ");
            highscores.highscore_table[i].options = 0x0000;
            for (int j=0; j<10; j++)
            {
                strcpy(highscores.highscore_table[i].scores[j].score, "000000");
                strcpy(highscores.highscore_table[i].scores[j].initials, "   ");
                strcpy(highscores.highscore_table[i].scores[j].reserved, "     ");                
                highscores.highscore_table[i].scores[j].year = 0;
                highscores.highscore_table[i].scores[j].month = 0;
                highscores.highscore_table[i].scores[j].day = 0;
            }
        }
        highscore_save();
    }
}


void highscore_save(void) 
{
    FILE *fp;

    DIR* dir = opendir("/data");
    if (dir)
    {
        /* Directory exists. */
        closedir(dir);
    }
    else
    {
        mkdir("/data", 0777);
    }
    
    // Ensure version and checksum are right...
    highscores.version = HS_VERSION;
    highscores.checksum = highscore_checksum();

    fp = fopen("/data/StellaDS.hi", "wb+");
    if (fp != NULL)
    {
        fwrite(&highscores, sizeof(highscores), 1, fp);
        fclose(fp);
    }
}

struct score_t score_entry;
char hs_line[33];
char md5[33];

void show_scores(short foundIdx)
{
    dsPrintValue(7,2,0, (char*)"** HIGH SCORES **");
    dsPrintValue(7,3,0, (char*)highscores.highscore_table[foundIdx].notes);
    for (int i=0; i<10; i++)
    {
        sprintf(hs_line, "%04d-%02d-%02d   %-3s   %-6s", highscores.highscore_table[foundIdx].scores[i].year, highscores.highscore_table[foundIdx].scores[i].month,highscores.highscore_table[foundIdx].scores[i].day, 
                                                         highscores.highscore_table[foundIdx].scores[i].initials, highscores.highscore_table[foundIdx].scores[i].score);
        dsPrintValue(3,6+i, 0, hs_line);
    }
    dsPrintValue(3,20,0, (char*)"PRESS X FOR NEW HI SCORE     ");
    dsPrintValue(3,21,0, (char*)"PRESS Y FOR NOTES            ");
    dsPrintValue(3,22,0, (char*)"PRESS B TO EXIT              ");
    dsPrintValue(3,23,0, (char*)"SCORES AUTO SORT AFTER ENTRY ");
    dsPrintValue(3,17,0, (char*)"                             ");
}

void highscore_entry(short foundIdx)
{
    char bEntryDone = 0;
    char blink=0;
    unsigned short entry_idx=0;
    char dampen=0;
    time_t unixTime = time(NULL);
    struct tm* timeStruct = gmtime((const time_t *)&unixTime);
    
    dsPrintValue(3,20,0, (char*)"UP/DN/LEFT/RIGHT ENTER SCORE");
    dsPrintValue(3,21,0, (char*)"PRESS START TO SAVE SCORE   ");
    dsPrintValue(3,22,0, (char*)"PRESS SELECT TO CANCEL      ");
    dsPrintValue(3,23,0, (char*)"                            ");

    strcpy(score_entry.score, "000000");
    strcpy(score_entry.initials, highscores.last_initials);
    score_entry.year  = timeStruct->tm_year +1900;
    score_entry.month = timeStruct->tm_mon+1;
    score_entry.day   = timeStruct->tm_mday;
    while (!bEntryDone)
    {
        swiWaitForVBlank();
        if (keysCurrent() & KEY_SELECT) {bEntryDone=1;}

        if (keysCurrent() & KEY_START) 
        {
            strcpy(highscores.last_initials, score_entry.initials);
            memcpy(&highscores.highscore_table[foundIdx].scores[9], &score_entry, sizeof(score_entry));
            strcpy(highscores.highscore_table[foundIdx].md5sum, md5);                    
            for (int i=0; i<9; i++)
            {
                for (int j=0; j<9; j++)
                {
                    if (strcmp(highscores.highscore_table[foundIdx].scores[j+1].score, highscores.highscore_table[foundIdx].scores[j].score) > 0)
                    {
                        // Swap...
                        memcpy(&score_entry, &highscores.highscore_table[foundIdx].scores[j], sizeof(score_entry));
                        memcpy(&highscores.highscore_table[foundIdx].scores[j], &highscores.highscore_table[foundIdx].scores[j+1], sizeof(score_entry));
                        memcpy(&highscores.highscore_table[foundIdx].scores[j+1], &score_entry, sizeof(score_entry));
                    }
                }
            }
            highscore_save();
            bEntryDone=1;
        }

        if (dampen == 0)
        {
            if ((keysCurrent() & KEY_RIGHT) || (keysCurrent() & KEY_A))
            {
                if (entry_idx < 8) entry_idx++; 
                blink=25;
                dampen=15;
            }

            if (keysCurrent() & KEY_LEFT)  
            {
                if (entry_idx > 0) entry_idx--; 
                blink=25;
                dampen=15;
            }

            if (keysCurrent() & KEY_UP)
            {
                if (entry_idx < 3)
                {
                    if (score_entry.initials[entry_idx] == ' ')
                        score_entry.initials[entry_idx] = 'A';
                    else if (score_entry.initials[entry_idx] == 'Z')
                        score_entry.initials[entry_idx] = ' ';
                    else score_entry.initials[entry_idx]++;
                }
                else
                {
                    score_entry.score[entry_idx-3]++;
                    if (score_entry.score[entry_idx-3] > '9') score_entry.score[entry_idx-3] = '0';
                }
                blink=0;
                dampen=10;
            }

            if (keysCurrent() & KEY_DOWN)
            {
                if (entry_idx < 3)
                {
                    if (score_entry.initials[entry_idx] == ' ')
                        score_entry.initials[entry_idx] = 'Z';
                    else if (score_entry.initials[entry_idx] == 'A')
                        score_entry.initials[entry_idx] = ' ';
                    else score_entry.initials[entry_idx]--;
                }
                else
                {
                    score_entry.score[entry_idx-3]--;
                    if (score_entry.score[entry_idx-3] < '0') score_entry.score[entry_idx-3] = '9';
                }
                blink=0;
                dampen=10;
            }
        }
        else
        {
            dampen--;
        }

        sprintf(hs_line, "%04d-%02d-%02d   %-3s   %-6s", score_entry.year, score_entry.month, score_entry.day, score_entry.initials, score_entry.score);
        if ((++blink % 60) > 30)
        {
            if (entry_idx < 3)
                hs_line[13+entry_idx] = '_';
            else
                hs_line[16+entry_idx] = '_';
        }
        dsPrintValue(3,17, 0, (char*)hs_line);
    }
    
    show_scores(foundIdx);
}

void highscore_options(short foundIdx)
{
    char notes[21];
    char bEntryDone = 0;
    char blink=0;
    unsigned short entry_idx=0;
    char dampen=0;
    
    dsPrintValue(3,20,0, (char*)"UP/DN/LEFT/RIGHT ENTER NOTES");
    dsPrintValue(3,21,0, (char*)"PRESS START TO SAVE NOTES   ");
    dsPrintValue(3,22,0, (char*)"PRESS SELECT TO CANCEL      ");
    dsPrintValue(3,23,0, (char*)"                            ");
    dsPrintValue(3,17,0, (char*)"NOTE: ");

    strcpy(notes, highscores.highscore_table[foundIdx].notes);
    while (!bEntryDone)
    {
        swiWaitForVBlank();
        if (keysCurrent() & KEY_SELECT) {bEntryDone=1;}

        if (keysCurrent() & KEY_START) 
        {
            strcpy(highscores.highscore_table[foundIdx].notes, notes);
            highscore_save();
            bEntryDone=1;
        }

        if (dampen == 0)
        {
            if ((keysCurrent() & KEY_RIGHT) || (keysCurrent() & KEY_A))
            {
                if (entry_idx < 19) entry_idx++; 
                blink=25;
                dampen=15;
            }

            if (keysCurrent() & KEY_LEFT)  
            {
                if (entry_idx > 0) entry_idx--; 
                blink=25;
                dampen=15;
            }

            if (keysCurrent() & KEY_UP)
            {
                if (notes[entry_idx] == ' ')
                    notes[entry_idx] = 'A';
                else if (notes[entry_idx] == 'Z')
                    notes[entry_idx] = '0';
                else if (notes[entry_idx] == '9')
                    notes[entry_idx] = ' ';
                else notes[entry_idx]++;
                blink=0;
                dampen=10;
            }

            if (keysCurrent() & KEY_DOWN)
            {
                if (notes[entry_idx] == ' ')
                    notes[entry_idx] = '9';
                else if (notes[entry_idx] == '0')
                    notes[entry_idx] = 'Z';
                else if (notes[entry_idx] == 'A')
                    notes[entry_idx] = ' ';
                else notes[entry_idx]--;
                blink=0;
                dampen=10;
            }
        }
        else
        {
            dampen--;
        }

        sprintf(hs_line, "%-20s", notes);
        if ((++blink % 60) > 30)
        {
            hs_line[entry_idx] = '_';
        }
        dsPrintValue(9,17, 0, (char*)hs_line);
    }
    
    show_scores(foundIdx);
}

void highscore_display(void) 
{
    short foundIdx = -1;
    short firstBlank = -1;
    char bDone = 0;

    decompress(bgHighScoreTiles, bgGetGfxPtr(bg0b), LZ77Vram);
    decompress(bgHighScoreMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
    dmaCopy((void *) bgHighScorePal,(u16*) BG_PALETTE_SUB,256*2);
    unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
    swiWaitForVBlank();

    // ---------------------------------------------------------------------------------
    // Get the current CART md5 so we can search for it in our High Score database...
    // ---------------------------------------------------------------------------------
    strcpy(md5, myCartInfo.md5.c_str());
    for (int i=0; i<MAX_HS_GAMES; i++)
    {
        if (firstBlank == -1)
        {
            if (strcmp("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", highscores.highscore_table[i].md5sum) == 0)
            {
                firstBlank = i;
            }
        }

        if (strcmp(md5, highscores.highscore_table[i].md5sum) == 0)
        {
            foundIdx = i;
            break;
        }
    }
    
    if (foundIdx == -1)
    {
        foundIdx = firstBlank;   
    }
    
    show_scores(foundIdx);

    while (!bDone)
    {
        if (keysCurrent() & KEY_A) bDone=1;
        if (keysCurrent() & KEY_B) bDone=1;
        if (keysCurrent() & KEY_X) highscore_entry(foundIdx);
        if (keysCurrent() & KEY_Y) highscore_options(foundIdx);
        
        // Clear the entire game of scores... 
        if ((keysCurrent() & KEY_L) && (keysCurrent() & KEY_R) && (keysCurrent() & KEY_Y))
        {
            strcpy(highscores.highscore_table[foundIdx].md5sum, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
            for (int j=0; j<10; j++)
            {
                strcpy(highscores.highscore_table[foundIdx].scores[j].score, "000000");
                strcpy(highscores.highscore_table[foundIdx].scores[j].initials, "   ");
                strcpy(highscores.highscore_table[foundIdx].scores[j].reserved, "    ");
                highscores.highscore_table[foundIdx].scores[j].year = 0;
                highscores.highscore_table[foundIdx].scores[j].month = 0;
                highscores.highscore_table[foundIdx].scores[j].day = 0;
            }
            show_scores(foundIdx);
            highscore_save();                    
        }
    }    
}


// End of file
