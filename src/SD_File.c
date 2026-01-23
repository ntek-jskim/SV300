/*-----------------------------------------------------------------------------
 *      RL-ARM - FlashFS
 *-----------------------------------------------------------------------------
 *      Name:    SD_File.c
 *      Purpose: File manipulation example program
 *-----------------------------------------------------------------------------
 *      This code is part of the RealView Run-Time Library.
 *      Copyright (c) 2004-2011 KEIL - An ARM Company. All rights reserved.
 *----------------------------------------------------------------------------*/

#include "board.h"
//#include <LPC43xx.h>                    /* LPC43xx Definitions                */
#include <RTL.h>                        /* RTL kernel functions & defines     */
#include <stdio.h>                      /* standard I/O .h-file               */
#include <ctype.h>                      /* character functions                */
#include <string.h>                     /* string and memory functions        */
#include "File_Config.h"
#include "Terminal.h"
//#include "Serial.h"
//#include "GLCD.h"
#include "time.h"
#include "stdlib.h"
#include "stdint.h"

/* Local variables */
static char in_line[160];

/* Local Function Prototypes */
static void dot_format (U64 val, char *sp);
static char *get_entry (char *cp, char **pNext);

/* Command definitions structure. */
typedef struct scmd {
  char val[10];
  void (*func)(char *par);
} SCMD;

#define __FI        1                   /* Font index 16x24                   */
#if (__FI == 1)                         /* Font index  6x8                    */                         
  #define __FONT_WIDTH  16
  #define __FONT_HEIGHT 24
#else                                   /* Font index 16x24                   */
  #define __FONT_WIDTH   6
  #define __FONT_HEIGHT  8
#endif

extern int		saveLockEnergy;

//
extern void writeGainU(int id);	 
extern void clrGainU(int id);
extern void setGainU(int id, float ref);

extern void writeGainI(int id);	 
extern void clrGainI(int id);
extern void setGainI(int id, float ref);

extern void writeGainPh(int id);	 
extern void clrGainPh(int id);
extern void setGainPh(int id);

extern void writeGainW(int id);	 
extern void clrGainW(int id);
extern void setGainW(int id, float vref, float iref);

extern void reqReboot(int);
extern void LcdBackLightUpDown(int d);

#ifdef  FLOW_RATE
extern void tdebug_on(void);
extern void tdebug_off(void);
#endif
//

/* Command Functions */
static void cmd_capture (char *par);
static void cmd_type (char *par);
static void cmd_rename (char *par);
static void cmd_copy (char *par);
static void cmd_delete (char *par);
static void cmd_dir (char *par);
static void cmd_format (char *par);
static void cmd_help (char *par);
static void cmd_fill (char *par);
static void cmd_v_thdoffset(char *par);
static void cmd_i_thdoffset(char *par);

static void cmd_vgain(char *par) {
	setGainU(0, 220);
}

static void cmd_igain(char *par) {
	setGainI(0, 1.6);
}

static void cmd_phgain(char *par) {
	setGainPh(0);
}

static void cmd_wgain(char *par) {
	setGainW(0, 220, 1.6);
}

static void cmd_clrvgain(char *par) {
	clrGainU(0);
}

static void cmd_clrigain(char *par) {
	clrGainI(0);
}

static void cmd_clrph(char *par) {
	clrGainPh(0);
}

static void cmd_clrwg(char *par) {
	clrGainW(0);
}


#ifdef FTP_WAVE

static int _active;
uint8_t ftpbuf[0x700000] __attribute__ ((section ("EXT_RAM"), zero_init)); 
int _filesize, _fileptr, _nleft;

void ftpc_notify (U8 event) {
  /* Result notification function. */

  _active = __FALSE;
  switch (event) {
    case FTPC_EVT_SUCCESS:
      printf ("Command successful\n");
      break;

    case FTPC_EVT_TIMEOUT:
      printf ("Failed, timeout expired\n");
      break;

    case FTPC_EVT_LOGINFAIL:
      printf ("Failed, username/password invalid\n");
      break;

    case FTPC_EVT_NOACCESS:
      printf ("Failed, operation not allowed\n");
      break;

    case FTPC_EVT_NOTFOUND:
      printf ("Failed, file or path not found\n");
      break;

    case FTPC_EVT_NOPATH:
      printf ("Failed, working directory not found\n");
      break;

    case FTPC_EVT_ERRLOCAL:
      printf ("Failed, local file open error\n");
      break;

    case FTPC_EVT_ERROR:
      printf ("Failed, unspecified protocol error\n");
      break;
  }
}


#define	FTPC_FILE 	"whatsup.mp3"

static void cmd_ftpc(char *par) {
	U8 srvip[4] = {192,168,8,71};
	FINFO info;
	FILE *fp;
	
	info.fileID = 0;      
	if (ffind (FTPC_FILE, &info)) {
		printf("Error on ffind ...\n");
		return;
	}
	
	_nleft = _filesize = info.size;	
	_fileptr = 0;
	
	fp = fopen(FTPC_FILE, "rb");	
	fread(ftpbuf, 1, info.size, fp);	
	fclose(fp);
	
	printf("--> ftp client, size = %d\n", _filesize);
	ftpc_connect (srvip, 0, FTPC_CMD_PUT, ftpc_notify);
}

#endif	// FTP_WAVE

//
void setDcOffset(void) {
	int i;
	pcntl->calDcOs = 1;
	for (i=0; i<5 && pcntl->calDcOs; i++) {
		os_dly_wait(100);
	}
	
	if (i<5) {
		printf("DC V OS: %d, %d, %d\n", pcal->vdcos[0], pcal->vdcos[1], pcal->vdcos[2]);
		printf("DC I OS: %d, %d, %d\n", pcal->idcos[0], pcal->idcos[1], pcal->idcos[2]);
	}
	else {
		printf("Can't get DC Offset ...\n");
	}
}

// void clrDcOffset(void) {
// 	int i=0; 
	
// 	for (i=0; i<3; i++) {
// 		pcal->vdcos[i] = pcal->idcos[i] = 0;
// 	}
// }

// static void cmd_dcos(char *par) {
// 	setDcOffset();
// }

// static void cmd_clrdcos(char *par) {
// 	clrDcOffset();
// }

static void cmd_datetime(char *par) {
	struct tm ltm;
  char *dt[6], *next;
	int i;
	time_t utc;
	
	for (i=0; i<6; i++) {
		dt[i] = get_entry (par, &next);
		if (dt[i] == NULL) {
			printf ("\nmissing argument ...\n");
			return;
		}
		par = next;
  }

	memset(&ltm, 0, sizeof(ltm));	
	ltm.tm_year = atoi(dt[0]) - 1900;
	ltm.tm_mon  = atoi(dt[1]) - 1;
	ltm.tm_mday = atoi(dt[2]);
	ltm.tm_hour = atoi(dt[3]);
	ltm.tm_min  = atoi(dt[4]);
	ltm.tm_sec  = atoi(dt[5]);	
	
	if (ltm.tm_year < 0 || ltm.tm_mon < 0) {
		printf("Invalid Year(%d) or Month(%d) ...\n", ltm.tm_year, ltm.tm_mon);
		return;
	}
	
	utc = mktime(&ltm);
	
	printf("[%d-%d-%d, %d:%d:%d] => {%d}\n", ltm.tm_year+1900, ltm.tm_mon+1, ltm.tm_mday, ltm.tm_hour, ltm.tm_min, ltm.tm_sec, utc);
	tickSet(utc, 0,1);
	RTC_SetTimeUTC(utc);
}


void manSetTime(uint16_t *dt) {
	struct tm ltm;
	time_t utc;
	
	memset(&ltm, 0, sizeof(ltm));	
	ltm.tm_year = dt[0] - 1900;
	ltm.tm_mon  = dt[1] - 1;
	ltm.tm_mday = dt[2];
	ltm.tm_hour = dt[3];
	ltm.tm_min  = dt[4];
	ltm.tm_sec  = dt[5];	
	
	if (ltm.tm_year < 0 || ltm.tm_mon < 0) {
		printf("Invalid Year(%d) or Month(%d) ...\n", ltm.tm_year, ltm.tm_mon);
		return;
	}
	
	utc = mktime(&ltm);
	
	printf("[%d-%d-%d, %d:%d:%d] => {%d}\n", ltm.tm_year+1900, ltm.tm_mon+1, ltm.tm_mday, ltm.tm_hour, ltm.tm_min, ltm.tm_sec, utc);
	tickSet(utc, 0, 1);
	RTC_SetTimeUTC(utc);	
}

static void cmd_meter(char *par) {
	printf("U : %.3f, %.3f, %.3f\n", pmeter->U[0], pmeter->U[1], pmeter->U[2]);
	printf("I : %.3f, %.3f, %.3f\n", pmeter->I[0], pmeter->I[1], pmeter->I[2]);
}

static void cmd_savecal(char *par) {
	storeHwSettings(pcal);
	printf("[[[Save HW Info & Cal. Data ...]]]\n");
}

static void cmd_initQual(char *par) {
	FILE *fp=fopen("initqual.d", "wb");
	int i=0x1234;
	if (fp != NULL) {
		fwrite(&i, sizeof(i), 1, fp);
		fclose(fp);
	}
}

#ifdef  FLOW_RATE
static void cmd_flowset(char *par) {
	char *next, *p;
	int temp, i;
	uint16_t ch, scale;
	
	
	for (i=0; i<2; i++) {
		p =	get_entry (par, &next);
		if (p != NULL) {
			sscanf(p, "%d", &temp);		
			if(i==0)
        ch = temp;
      else
        scale = temp;

			par = next;
		}
		else {
			break;
		}
	}
  pmeter->flowScale[ch] = scale;

  printf("Flow Scale[%d] = %d(%d)\n", ch, scale, pmeter->flowScale[ch]);

}
#endif

// macset : 십진수로 입력한다 
static void cmd_macset(char *par) {
	char *next, *p;
	int temp, i;
	uint8_t mac[]={0,0,0,0};
	
	
	for (i=0; i<4; i++) {
		p =	get_entry (par, &next);
		if (p != NULL) {
			sscanf(p, "%d", &temp);		
			mac[i] = temp;
			par = next;
		}
		else {
			break;
		}
	}
	
	if (i == 4) {	
		for (i=0; i<4; i++) pcal->mac[i] = mac[i];
		
		storeHwSettings(pcal);
		printf("MAC Address = %d:%d:%d:%d (%02x:%02x:%02x:%02x)\n", 
			pcal->mac[0], pcal->mac[1], pcal->mac[2], pcal->mac[3],
			pcal->mac[0], pcal->mac[1], pcal->mac[2], pcal->mac[3]);
	}
	else {
		printf("*** not enough input arguments ...\n");
	}
}

static void cmd_setwh(char *par) {
	char *p;
	int temp, i, k1, k2, k3;
  uint64_t  k4;
  char *dt[4], *next;
	
	for (i=0; i<4; i++) {
		dt[i] = get_entry (par, &next);
		if (dt[i] == NULL) {
			printf ("\nmissing argument ...\n");
			return;
		}
		par = next;
  }
	
  k1 = atoi(dt[0]);
  k2 = atoi(dt[1]);
  k3 = atoi(dt[2]);
  k4 = strtoull(dt[3], NULL, 10);

  if(k1>2 || k2>2 || k3 > 1) {
		printf ("\nmissing argument [%d][%d][%d]...\n", k1, k2, k3);
    return;
  }
  else {
    saveLockEnergy = 1;
    egyNvr.Ereg64[k1].eh[k2][k3] = k4;
//    copyEreg32(&egyNvr);
//    storeEnergy(&egyNvr);
    printf("Stored: [%d][%d][%d] = %lld\n", k1, k2, k3, (unsigned long long)k4);
    saveLockEnergy = 0;
  }

}
static void cmd_savewh(char *par) {
  copyEreg32(&egyNvr);
  storeEnergy();
	reqReboot(0x1234);

}

static void cmd_reboot(char *par) {
	reqReboot(0x1234);	// smb_rtu.c에 선언
}

static void cmd_blup(char *par) {
	LcdBackLightUpDown(10);
}

static void cmd_bldown(char *par) {
	LcdBackLightUpDown(-10);
}

static void cmd_hwModel(char *par) {
	char *p, *next;

	p = get_entry (par, &next);
	if (p != NULL) {
		sscanf(p, "%d", &pcal->hwModel);
		printf("HwModel = %d\n", pcal->hwModel);
		storeHwSettings(pcal);
	}
	else {
		printf("*** not enough input arguments ...\n");
	}
}

static void cmd_hwVersion(char *par) {
	char *p, *next;

	p = get_entry (par, &next);
	if (p != NULL) {
		sscanf(p, "%d", &pcal->hwVer);
		printf("HwVersion = %d\n", pcal->hwVer);
		storeHwSettings(pcal);
	}
	else {
		printf("*** not enough input arguments ...\n");
	}	
}

static void cmd_devInfo(char *par) {
	printf("FW VERSION = %02x.%02x\n", pInfo->fwVer >> 8, pInfo->fwVer & 0xff);
	printf("HW MODE    = %d\n", pcal->hwModel);
	printf("HW VERSION = %d\n", pcal->hwVer);
	printf("HW MACress = %d:%d:%d:%d (%02x:%02x:%02x:%02x)\n", 
			pcal->mac[0], pcal->mac[1], pcal->mac[2], pcal->mac[3],
			pcal->mac[0], pcal->mac[1], pcal->mac[2], pcal->mac[3]);
}

void cmd_v_thdoffset(char *par)
{
   char *hw1;
	float	val1 =0;

  hw1 = get_entry (par, &hw1);
  if (hw1 == NULL) {
    printf ("\nwh offset missing.\n");
    return;
  }
	val1 = atof(hw1);

  pcal->v_thd_offset = val1/10;
 	printf("V THD OFFSET = [%.1f]\n", pcal->v_thd_offset);
  storeHwSettings(pcal);
//  saveDB();
}
void cmd_i_thdoffset(char *par)
{
   char *hw1;
	float	val1 =0;

  hw1 = get_entry (par, &hw1);
  if (hw1 == NULL) {
    printf ("\nwh offset missing.\n");
    return;
  }
	val1 = atof(hw1);

  pcal->i_thd_offset = val1/10;
 	printf("I THD OFFSET = [%.1f]\n", pcal->i_thd_offset);
  storeHwSettings(pcal);
//  saveDB();
}


// 
// hard fault test
//[Hard fault handler - all numbers in hex]
//R0 = 0
//R1 = 1
//R2 = 0
//R3 = 1d9bf
//R12 = 1a0276c1
//LR [R14] = 1a0297eb  subroutine call return address
//PC [R15] = 1a0297ea  program counter
//PSR = 61000000
//BFAR = e000ed38
//CFSR = 400
//HFSR = 40000000
//DFSR = 0
//AFSR = 0
//SCB_SHCSR = 0

static void cmd_hardfault(char *par) {
	*(uint32_t *)0 = 1;
}

static void cmd_ethrst(char *par) {
	Chip_GPIO_SetPinOutLow (LPC_GPIO_PORT, 4, 12);
	os_dly_wait(10);
	Chip_GPIO_SetPinOutHigh (LPC_GPIO_PORT, 4, 12);
	printf("---> EthRST ...\n");
}

static void cmd_TrVTrg(char *par) {
	pcntl->TrVTrg = 1;
}

static void cmd_TrCTrg(char *par) {
	pcntl->TrCTrg = 1;
}


/* Local constants */
static const char intro[] =
  "\n\n\n\n\n\n\n\n"
  "+-----------------------------------------------------------------------+\n"
  "|                SD/MMC Card File Manipulation example                  |\n";

static const char help[] = 
  "+ command ------------------+ function ---------------------------------+\n"
  "| CAP \"fname\" [/A]          | captures serial data to a file            |\n"
  "|                           |  [/A option appends data to a file]       |\n"
  "| FILL \"fname\" [nnnn]       | create a file filled with text            |\n"
  "|                           |  [nnnn - number of lines, default=1000]   |\n"
  "| TYPE \"fname\"              | displays the content of a text file       |\n"
  "| REN \"fname1\" \"fname2\"     | renames a file 'fname1' to 'fname2'       |\n"
  "| COPY \"fin\" [\"fin2\"] \"fout\"| copies a file 'fin' to 'fout' file        |\n"
  "|                           |  ['fin2' option merges 'fin' and 'fin2']  |\n"
  "| DEL \"fname\"               | deletes a file                            |\n"
  "| DIR [\"mask\"]              | displays a list of files in the directory |\n"
  "| FORMAT [label [/FAT32]]   | formats the device                        |\n"
  "|                           | [/FAT32 option selects FAT32 file system] |\n"
  "| HELP  or  ?               | displays this help                        |\n"
  "+---------------------------+-------------------------------------------+\n";

static const SCMD cmd[] = {
  "CAP",    cmd_capture,
  "TYPE",   cmd_type,
  "REN",    cmd_rename,
  "COPY",   cmd_copy,
  "DEL",    cmd_delete,
  "DIR",    cmd_dir,
  "FORMAT", cmd_format,
  "HELP",   cmd_help,
  "FILL",   cmd_fill,
  "?",      cmd_help,
	"METER",	cmd_meter,
	"VG",	cmd_vgain,
	"IG", cmd_igain,
	"PH", cmd_phgain,
	"WG", cmd_wgain,
	"CLRVG", cmd_clrvgain,
	"CLRIG", cmd_clrigain,
	"CLRPH", cmd_clrph,
	"CLRWG", cmd_clrwg,
	"SAVECAL", cmd_savecal,
	"CLROS", cmd_clrdcos,
	"DCOS",  cmd_dcos,
	"DATETIME", cmd_datetime,
	"INITQUAL", cmd_initQual,
	"MACSET", cmd_macset,
	"MODEL", cmd_hwModel,
	"HWVERSION", cmd_hwVersion,
	"DEVINFO", cmd_devInfo,
	"REBOOT", cmd_reboot,
	"BL+", cmd_blup,
	"BL-", cmd_bldown,
	"ETHRST", cmd_ethrst,
	"TRVTRG", cmd_TrVTrg,
	"TRCTRG", cmd_TrCTrg,
	"FDON", tdebug_on,
	"FDOFF", tdebug_off,
//	"FLOWSET", cmd_flowset,
  "VTHDSET", cmd_v_thdoffset, 
  "ITHDSET", cmd_i_thdoffset, 
  "SETWH", cmd_setwh, 
  "SAVEWH", cmd_savewh, 

#ifdef	FTP_WAVE
	"FTPC", cmd_ftpc
#endif
};

#define CMD_COUNT   (sizeof (cmd) / sizeof (cmd[0]))

/*-----------------------------------------------------------------------------
 *        Initialize On Board LCD Module
 *----------------------------------------------------------------------------*/
static void init_display () {
//  /* LCD Module init */

//  GLCD_Init ();
//  GLCD_Clear        (Blue);
//  GLCD_SetBackColor (Blue);
//  GLCD_SetTextColor (White);
//  GLCD_DisplayString (0, 0, __FI, "      MCB4300       ");
//  GLCD_DisplayString (1, 0, __FI, "  Evaluation Board  ");
//  GLCD_DisplayString (3, 0, __FI, "       RL-ARM       ");
//  GLCD_DisplayString (4, 0, __FI, "  SD_File example   ");
}

/*-----------------------------------------------------------------------------
 *        Process input string for long or short name entry
 *----------------------------------------------------------------------------*/
static char *get_entry (char *cp, char **pNext) {
  char *sp, lfn = 0, sep_ch = ' ';

  if (cp == NULL) {                           /* skip NULL pointers           */
    *pNext = cp;
    return (cp);
  }

  for ( ; *cp == ' ' || *cp == '\"'; cp++) {  /* skip blanks and starting  "  */
    if (*cp == '\"') { sep_ch = '\"'; lfn = 1; }
    *cp = 0;
  }
 
  for (sp = cp; *sp != CR && *sp != LF && *sp != 0; sp++) {
    if ( lfn && *sp == '\"') break;
    if (!lfn && *sp == ' ' ) break;
  }

  for ( ; *sp == sep_ch || *sp == CR || *sp == LF; sp++) {
    *sp = 0;
    if ( lfn && *sp == sep_ch) { sp ++; break; }
  }

  *pNext = (*sp) ? sp : NULL;                 /* next entry                   */
  return (cp);
}

/*-----------------------------------------------------------------------------
 *        Print size in dotted fomat
 *----------------------------------------------------------------------------*/
static void dot_format (U64 val, char *sp) {

  if (val >= (U64)1e12) {
    sp += sprintf (sp,"%d.",(U32)(val/(U64)1e12));
    val %= (U64)1e12;
    sp += sprintf (sp,"%03d.",(U32)(val/(U64)1e9));
    val %= (U64)1e9;
    sp += sprintf (sp,"%03d.",(U32)(val/(U64)1e6));
    val %= (U64)1e6;
    sprintf (sp,"%03d.%03d",(U32)(val/1000),(U32)(val%1000));
    return;
  }
  if (val >= (U64)1e9) {
    sp += sprintf (sp,"%d.",(U32)(val/(U64)1e9));
    val %= (U64)1e9;
    sp += sprintf (sp,"%03d.",(U32)(val/(U64)1e6));
    val %= (U64)1e6;
    sprintf (sp,"%03d.%03d",(U32)(val/1000),(U32)(val%1000));
    return;
  }
  if (val >= (U64)1e6) {
    sp += sprintf (sp,"%d.",(U32)(val/(U64)1e6));
    val %= (U64)1e6;
    sprintf (sp,"%03d.%03d",(U32)(val/1000),(U32)(val%1000));
    return;
  }
  if (val >= 1000) {
    sprintf (sp,"%d.%03d",(U32)(val/1000),(U32)(val%1000));
    return;
  }
  sprintf (sp,"%d",(U32)(val));
}

/*-----------------------------------------------------------------------------
 *        Capture serial data to file
 *----------------------------------------------------------------------------*/
static void cmd_capture (char *par) {
  char *fname,*next;
  BOOL append,esc;
  U32  cnt;
  FILE *f;

  fname = get_entry (par, &next);
  if (fname == NULL) {
    printf ("\nFilename missing.\n");
    return;
  }
  append = __FALSE;
  if (next) {
    par = get_entry (next, &next);
    if ((strcmp (par, "/A") == 0) ||(strcmp (par, "/a") == 0)) {
      append = __TRUE;
    }
    else {
      printf ("\nCommand error.\n");
      return;
    }
  }
  printf ((append) ? "\nAppend data to file %s" :
                     "\nCapture data to file %s", fname);
  printf("\nPress ESC to stop.\n");
  f = fopen (fname,append ? "a" : "w"); /* open a file for writing            */
  if (f == NULL) {
    printf ("\nCan not open file!\n");  /* error when trying to open file     */
    return;
  }
  esc = __FALSE;
  do {
    cnt = getline (in_line, sizeof (in_line));
    if (cnt) {
      if (in_line[cnt-1] == ESC) {
        in_line[cnt-1] = 0;
        esc = __TRUE;
      }
      fputs (in_line, f);
    }
  } while (esc == __FALSE);
  fclose (f);                         /* close the output file                */
  printf ("\nFile closed.\n");
}

/*-----------------------------------------------------------------------------
 *        Create a file and fill it with some text
 *----------------------------------------------------------------------------*/
static void cmd_fill (char *par) {
  char *fname, *next;
  FILE *f;
  int i,cnt = 1000;

  fname = get_entry (par, &next);
  if (fname == NULL) {
    printf ("\nFilename missing.\n");
    return;
  }
  if (next) {
    par = get_entry (next, &next);
    if (sscanf (par,"%d", &cnt) == 0) {
      printf ("\nCommand error.\n");
      return;
    }
  }

  f = fopen (fname, "w");               /* open a file for writing            */
  if (f == NULL) {
    printf ("\nCan not open file!\n");  /* error when trying to open file     */
    return;
  } 
  for (i = 0; i < cnt; i++)  {
    fprintf (f, "This is line # %d in file %s\n", i, fname);
    if (!(i & 0x3FF)) printf("."); fflush (stdout);
  }
  fclose (f);                           /* close the output file              */
  printf ("\nFile closed.\n");
}

/*-----------------------------------------------------------------------------
 *        Read file and dump it to serial window
 *----------------------------------------------------------------------------*/
static void cmd_type (char *par) {
  char *fname,*next;
  FILE *f;
  int ch;

  fname = get_entry (par, &next);
  if (fname == NULL) {
    printf ("\nFilename missing.\n");
    return;
  }
  printf("\nRead data from file %s\n",fname);
  f = fopen (fname,"r");                /* open the file for reading          */
  if (f == NULL) {
    printf ("\nFile not found!\n");
    return;
  }
 
  while ((ch = fgetc (f)) != EOF) {     /* read the characters from the file  */
    putchar (ch);                       /* and write them on the screen       */
  }
  fclose (f);                           /* close the input file when done     */
  printf ("\nFile closed.\n");
}

/*-----------------------------------------------------------------------------
 *        Rename a File
 *----------------------------------------------------------------------------*/
static void cmd_rename (char *par) {
  char *fname,*fnew,*next,dir;

  fname = get_entry (par, &next);
  if (fname == NULL) {
    printf ("\nFilename missing.\n");
    return;
  }
  fnew = get_entry (next, &next);
  if (fnew == NULL) {
    printf ("\nNew Filename missing.\n");
    return;
  }
  if (strcmp (fname,fnew) == 0) {
    printf ("\nNew name is the same.\n");
    return;
  }

  dir = 0;
  if (*(fname + strlen(fname) - 1) == '\\') {
    dir = 1;
  }

  if (frename (fname, fnew) == 0) {
    if (dir) {
      printf ("\nDirectory %s renamed to %s\n",fname,fnew);
    }
    else {
      printf ("\nFile %s renamed to %s\n",fname,fnew);
    }
  }
  else {
    if (dir) {
      printf ("\nDirectory rename error.\n");
    }
    else {
      printf ("\nFile rename error.\n");
    }
  }
}

/*-----------------------------------------------------------------------------
 *        Copy a File
 *----------------------------------------------------------------------------*/
static void cmd_copy (char *par) {
  char *fname,*fnew,*fmer,*next;
  FILE *fin,*fout;
  U32 cnt,total;
  char buf[512];
  BOOL merge;

  fname = get_entry (par, &next);
  if (fname == NULL) {
    printf ("\nFilename missing.\n");
    return;
  }
  fmer = get_entry (next, &next);
  if (fmer == NULL) {
    printf ("\nNew Filename missing.\n");
    return;
  }
  fnew = get_entry (next, &next);
  if (fnew != NULL) {
    merge = __TRUE;
  }
  else {
    merge = __FALSE;
    fnew = fmer;
  }
  if ((strcmp (fname,fnew) == 0)        ||
      (merge && strcmp (fmer,fnew) == 0)) {
    printf ("\nNew name is the same.\n");
    return;
  }

  fin = fopen (fname,"r");              /* open the file for reading          */
  if (fin == NULL) {
    printf ("\nFile %s not found!\n",fname);
    return;
  }

  if (merge == __FALSE) {
    printf ("\nCopy file %s to %s\n",fname,fnew);
  }
  else {
    printf ("\nCopy file %s, %s to %s\n",fname,fmer,fnew);
  }
  fout = fopen (fnew,"w");              /* open the file for writing          */
  if (fout == NULL) {
    printf ("\nFailed to open %s for writing!\n",fnew);
    fclose (fin);
    return;
  }

  total = 0;
  while ((cnt = fread (&buf, 1, 512, fin)) != 0) {
    fwrite (&buf, 1, cnt, fout);
    total += cnt;
  }
  fclose (fin);                         /* close input file when done         */

  if (merge == __TRUE) {
    fin = fopen (fmer,"r");             /* open the file for reading          */
    if (fin == NULL) {
      printf ("\nFile %s not found!\n",fmer);
    }
    else {
      while ((cnt = fread (&buf, 1, 512, fin)) != 0) {
        fwrite (&buf, 1, cnt, fout);
        total += cnt;
      }
      fclose (fin);
    }
  }
  fclose (fout);
  dot_format (total, &buf[0]);
  printf ("\n%s bytes copied.\n", &buf[0]);
}

/*-----------------------------------------------------------------------------
 *        Delete a File
 *----------------------------------------------------------------------------*/
static void cmd_delete (char *par) {
  char *fname,*next,dir;

  fname = get_entry (par, &next);
  if (fname == NULL) {
    printf ("\nFilename missing.\n");
    return;
  }

  dir = 0;
  if (*(fname + strlen(fname) - 1) == '\\') {
    dir = 1;
  }

  if (fdelete (fname) == 0) {
    if (dir) {
      printf ("\nDirectory %s deleted.\n",fname);
    }
    else {
      printf ("\nFile %s deleted.\n",fname);
    }
  }
  else {
    if (dir) {
      printf ("\nDirectory %s not found or not empty.\n",fname);
    }
    else {
      printf ("\nFile %s not found.\n",fname);
    }
  }
}

/*-----------------------------------------------------------------------------
 *        Print a Directory
 *----------------------------------------------------------------------------*/
// 사용법
// list : dir
// 특정 Directory list : dir \DirectoryName\*
static void cmd_dir (char *par) {
  U64 fsize;
  U32 files,dirs,i;
  char temp[32],*mask,*next,ch;
  FINFO info;

  mask = get_entry (par, &next);
  if (mask == NULL) {
    mask = "*.*";
  } else if ((mask[1] == ':') && (mask[2] == 0)) {
    mask[2] = '*'; 
    mask[3] = '.'; 
    mask[4] = '*'; 
    mask[5] = 0; 
  } else if ((mask[2] == ':') && (mask[3] == 0)) {
    mask[3] = '*'; 
    mask[4] = '.'; 
    mask[5] = '*'; 
    mask[6] = 0; 
  }

  printf ("\nFile System Directory...");
  files = 0;
  dirs  = 0;
  fsize = 0;
  info.fileID  = 0;
  while (ffind (mask,&info) == 0) {
    if (info.attrib & ATTR_DIRECTORY) {
      i = 0;
      while (strlen((const char *)info.name+i) > 41) {
        ch = info.name[i+41];
        info.name[i+41] = 0;
        printf ("\n%-41s", &info.name[i]);
        info.name[i+41] = ch;
        i += 41;
      }
      printf ("\n%-41s    <DIR>       ", &info.name[i]);
      printf ("  %02d.%02d.%04d  %02d:%02d",
               info.time.day, info.time.mon, info.time.year,
               info.time.hr, info.time.min);
      dirs++;
    }
    else {
      dot_format (info.size, &temp[0]);
      i = 0;
      while (strlen((const char *)info.name+i) > 41) {
        ch = info.name[i+41];
        info.name[i+41] = 0;
        printf ("\n%-41s", &info.name[i]);
        info.name[i+41] = ch;
        i += 41;
      }
      printf ("\n%-41s %14s ", &info.name[i], temp);
      printf ("  %02d.%02d.%04d  %02d:%02d",
               info.time.day, info.time.mon, info.time.year,
               info.time.hr, info.time.min);
      fsize += info.size;
      files++;
    }
  }
  if (info.fileID == 0) {
    printf ("\nNo files...");
  }
  else {
    dot_format (fsize, &temp[0]);
    printf ("\n              %9d File(s)    %21s bytes", files, temp);
  }
  dot_format (ffree(mask), &temp[0]);
  if (dirs) {
    printf ("\n              %9d Dir(s)     %21s bytes free.\n", dirs, temp);
  }
  else {
    printf ("\n%56s bytes free.\n",temp);
  }
}

/*-----------------------------------------------------------------------------
 *        Format Device
 *----------------------------------------------------------------------------*/
static void cmd_format (char *par) {
  char *label,*next,*opt;
  char arg[20];
  U32 retv;

  label = get_entry (par, &next);
  if (label == NULL) {
    label = "KEIL";
  }
  strcpy (arg, label);
  opt = get_entry (next, &next);
  if (opt != NULL) {
    if ((strcmp (opt, "/FAT32") == 0) ||(strcmp (opt, "/fat32") == 0)) {
      strcat (arg, "/FAT32");
    }
  }
  printf ("\nFormat Flash Memory Card? [Y/N]\n");
  while ((retv = getkey()) == 0);
  if (retv == 'y' || retv == 'Y') {
    /* Format the Device with Label "KEIL". "*/
    if (fformat (arg) == 0) {
      printf ("Memory Card Formatted.\n");
      printf ("Card Label is %s\n",label);
    }
    else {
      printf ("Formatting failed.\n");
    }
  }
}

/*-----------------------------------------------------------------------------
 *        Display Command Syntax help
 *----------------------------------------------------------------------------*/
static void cmd_help (char *par) {
  printf (help);
}

/*-----------------------------------------------------------------------------
 *        Initialize a Flash Memory Card
 *----------------------------------------------------------------------------*/
static void init_card (void) {
  U32 retv;

  while ((retv = finit (NULL)) != 0) {        /* Wait until the Card is ready */
    if (retv == 1) {
      printf ("\nSD/MMC Init Failed");
      printf ("\nInsert Memory card and press key...\n");
      while (getkey () == 0);
    }
    else {
      printf ("\nSD/MMC Card is Unformatted");
      strcpy (&in_line[0], "KEIL\r\n");
      cmd_format (&in_line[0]);
    }
  }
}

/*-----------------------------------------------------------------------------
 *        Main: 
 *----------------------------------------------------------------------------*/
//int main (void) {
//  BOOL prompt = __TRUE;
//  char *sp,*cp,*next;
//  U32 i;

//  SER_Init ();                                  /* init serial interface      */
//  init_display ();
//  printf (intro);                               /* display example info       */
//  printf (help);

//  init_card ();
//  while (1) {
//    if (prompt) {
//      printf ("\nCmd> ");                       /* display prompt             */
//      fflush (stdout);
//    }
//                                                /* get command line input     */
//    if (getline (in_line, sizeof (in_line))) {
//      sp = get_entry (&in_line[0], &next);
//      if (*sp == 0) {
//        continue;
//      }

//      for (cp = sp; *cp && *cp != ' '; cp++) {
//        *cp = toupper (*cp);                    /* command to upper-case      */
//      }
//      for (i = 0; i < CMD_COUNT; i++) {
//        if (strcmp (sp, (const char *)&cmd[i].val)) {
//          continue;
//        }
//        init_card();                            /* check if card is removed   */
//        cmd[i].func (next);                     /* execute command function   */
//        break;
//      }
//      if (i == CMD_COUNT) {
//        printf ("\nCommand error\n");
//      }
//      prompt = __TRUE;
//    }
//    else prompt = __FALSE;
//  }
//}


// FS Structure
//



void init_directory() {
	FILE *fp;
	uint32_t init=0;
	char path[64];
	
	sprintf(path, "init.ini");
	fp = fopen(path, "rb");
	if (fp == NULL) {
		printf("{{Can't open Init File(%s)}}\n", path);
		
		fp = fopen(CONCAT(SYS_DIR, TEMP_FILE), "wb");
		fclose(fp);
		printf("[[Create Directory(%s)]]\n", CONCAT(SYS_DIR, TEMP_FILE));
		
		fp = fopen(CONCAT(LOG_PQ_DIR, TEMP_FILE), "wb");
		fclose(fp);
		printf("[[Create Directory(%s)]]\n", CONCAT(LOG_PQ_DIR, TEMP_FILE));
		
		fp = fopen(CONCAT(LOG_TREND_DIR, TEMP_FILE), "wb");
		fclose(fp);
		printf("[[Create Directory(%s)]]\n", CONCAT(LOG_TREND_DIR, TEMP_FILE));
		
		fp = fopen(CONCAT(TRG_PQ_DIR, TEMP_FILE), "wb");
		fclose(fp);
		printf("[[Create Directory(%s)]]\n", CONCAT(TRG_PQ_DIR, TEMP_FILE));
		
		fp = fopen(CONCAT(TRG_TRANSIENT_DIR, TEMP_FILE), "wb");
		fclose(fp);
		printf("[[Create Directory(%s)]]\n", CONCAT(TRG_TRANSIENT_DIR, TEMP_FILE));
		
		fp = fopen(CONCAT(FW_DIR, TEMP_FILE), "wb");
		fclose(fp);		
		printf("[[Create Directory(%s)]]\n", CONCAT(FW_DIR, TEMP_FILE));
		
		fp = fopen(CONCAT(ALARM_DIR, TEMP_FILE), "wb");
		fclose(fp);
		printf("[[Create Directory(%s)]]\n", CONCAT(ALARM_DIR, TEMP_FILE));
		
		fp = fopen(CONCAT(EVENT_DIR, TEMP_FILE), "wb");
		fclose(fp);			
		printf("[[Create Directory(%s)]]\n", CONCAT(EVENT_DIR, TEMP_FILE));
		
		init=1;
		fp = fopen(F_INIT, "wb");
		fwrite(&init, sizeof(init), 1, fp);
		fclose(fp);
		
		printf("[[Create System Directory, init=%d...]]\n", init);
	}	
	else {
		fread(&init, sizeof(int), 1, fp);
		fclose(fp);
		
		printf("[[Init Mode = %d\n", init);
	}
}


void FS_Init() {
  printf (intro);                               /* display example info       */
  printf (help);
  init_card ();	
	init_directory();
}

void shell (void) {
  static BOOL prompt = __TRUE;
  static char *sp,*cp,*next;
  static U32 i;

	//while (1) {
    if (prompt) {
      printf ("\nCmd> ");                       /* display prompt             */
      fflush (stdout);
    }
                                                /* get command line input     */
    if (getline (in_line, sizeof (in_line))) {
      sp = get_entry (&in_line[0], &next);
      if (*sp == 0) {
				prompt = __TRUE;
        return;
      }

      for (cp = sp; *cp && *cp != ' '; cp++) {
        *cp = toupper (*cp);                    /* command to upper-case      */
      }
      for (i = 0; i < CMD_COUNT; i++) {
        if (strcmp (sp, (const char *)&cmd[i].val)) {
          continue;
        }
        init_card();                            /* check if card is removed   */
        cmd[i].func (next);                     /* execute command function   */
        break;
      }
      if (i == CMD_COUNT) {
        printf ("\nCommand error\n");
      }
      prompt = __TRUE;
    }
    else {
			prompt = __FALSE;
		}
		
	//}
}


/*-----------------------------------------------------------------------------
 * end of file
 *----------------------------------------------------------------------------*/
