#if 1

#include <RTL.h>  
#include "stdio.h"
#include "string.h"
#include "File_Config.h"
#include "terminal.h"
#include "stdlib.h"
#include "time.h"
#include "meter.h"
#include "debug.h"
#include "os_port.h"

static uint8_t s_fsMounted;
void init_directory(void);

extern int  debug_getchar();
extern int readchar();
extern void SystemCoreClockUpdate();
extern void selectMeter(int id);
extern void deSelectMeter(int id);
extern uint32_t RTC_GetTimeUTC(void);
#include "ade9000.h"	/* ADE_STATUS, getAdeStatus() */

/* Local variables */
static char in_line[160];

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
  while ((retv = readchar()) == 0);
  if (retv == 'y' || retv == 'Y') {
    int fmtOk;

    /* Meter/Alarm/PostScan 동시 fopen 시 FAT 손상 — FORMAT 구간만 전 태스크 정지 */
    fsFileLock();
    osSuspendAllTasks();
    fmtOk = (fformat(arg) == 0);
    if (fmtOk) {
      s_fsMounted = 1;
      init_directory();
    } else {
      s_fsMounted = 0;
    }
    osResumeAllTasks();
    fsFileUnlock();

    if (fmtOk) {
      printf ("Memory Card Formatted.\n");
      printf ("Card Label is %s\n",label);
    }
    else {
      printf ("Formatting failed.\n");
    }
  }
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

  fsFileLock();
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
  fsFileUnlock();
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
  fsFileLock();
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
  fsFileUnlock();
}




static void cmd_initQual(char *par) {
	FILE *fp=fopen("initqual.d", "wb");
	int i=0x1234;
	if (fp != NULL) {
		fwrite(&i, sizeof(i), 1, fp);
		fclose(fp);
	}
}


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

static void cmd_dcos(char *par) {
	setDcOffset(0);
	setDcOffset(1);
}

static void cmd_clrdcos(char *par) {
  clrDcOffset(0);
	clrDcOffset(1);
}

static void cmd_help (char *par) {
  printf (help);
}


static void cmd_datetime(char *par) {
	struct tm ltm;
	char *dt[6], *next;
	int i;
	time_t utc;
	uint32_t rtcUtc;

	dt[0] = get_entry(par, &next);
	if (par == NULL || *par == 0 || dt[0] == NULL || dt[0][0] == 0) {
		uLocalTime(&sysTick1s, &ltm);
		printf("Current (app): %04d-%02d-%02d %02d:%02d:%02d  unix=%lu\n",
		       ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday,
		       ltm.tm_hour, ltm.tm_min, ltm.tm_sec,
		       (unsigned long)sysTick1s);
		rtcUtc = RTC_GetTimeUTC();
		uLocalTime(&rtcUtc, &ltm);
		printf("Current (RTC): %04d-%02d-%02d %02d:%02d:%02d  unix=%lu\n",
		       ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday,
		       ltm.tm_hour, ltm.tm_min, ltm.tm_sec,
		       (unsigned long)rtcUtc);
		return;
	}

	par = next;
	for (i = 1; i < 6; i++) {
		if (par == NULL) {
			printf("\nDATETIME: need 6 integers (year month day hour min sec), or no args to show time.\n");
			return;
		}
		dt[i] = get_entry(par, &next);
		if (dt[i] == NULL || dt[i][0] == 0) {
			printf("\nDATETIME: need 6 integers (year month day hour min sec), or no args to show time.\n");
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

	printf("[%d-%d-%d, %d:%d:%d] => {%d}\n", ltm.tm_year+1900, ltm.tm_mon+1, ltm.tm_mday, ltm.tm_hour, ltm.tm_min, ltm.tm_sec, (int)utc);
	tickSet((uint32_t)utc, 0, 1);
	RTC_SetTimeUTC((uint32_t)utc);
}
// macset : 십진수로 입력한다 
// 2025-3-10, mac주소는 ChipId로 사용하고 실제입력한 값은 일련번호로 사용한다
static void cmd_macset(char *par) {
	char *next, *p;
	int temp, i;
	uint8_t mac[]={0,0,0,0};
	uint32_t sn[2];
	
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
#if 1
    pcal->sn[0] = pcal->hwVer & 0x0000ffff;	// hwModel 미사용(삭제)
    pcal->sn[1] = (mac[0]<<24)&0xff000000;
    pcal->sn[1] |= (mac[1]<<16)&0x00ff0000;
    pcal->sn[1] |= (mac[2]<<8)&0x0000ff00;
    pcal->sn[1] |= (mac[3])&0x000000ff;

    storeHwSettings(pcal);
		printf("sn Address = %u:%u:%u:%u:%u:%u)\n", 
			pcal->sn[0]>>16, pcal->sn[0]&0xffff, (pcal->sn[1]>>24)&0xff, (pcal->sn[1]>>16)&0xff,(pcal->sn[1]>>8)&0xff,(pcal->sn[1])&0xff);
#else
	for (i=0; i<4; i++) pcal->mac[i] = mac[i];
		
	storeHwSettings(pcal);
	printf("MAC Address = %d:%d:%d:%d (%02x:%02x:%02x:%02x)\n", 
		pcal->mac[0], pcal->mac[1], pcal->mac[2], pcal->mac[3],
		pcal->mac[0], pcal->mac[1], pcal->mac[2], pcal->mac[3]);
#endif    
	}
	else {
		printf("*** not enough input arguments ...\n");
	}
}


static void cmd_hwModel(char *par) {
	char *p, *next;

	p = get_entry (par, &next);
	if (p != NULL) {
		pcal->hwModel = (uint16_t)atoi(p);		/* 0=3CH, 1=2CH (추후 HV/Rogowski 등 확장) */
		storeHwSettings(pcal);					/* METER_CAL(FRAM)에 저장 */
		printf("HwModel = %d (%s) saved\n", pcal->hwModel, (pcal->hwModel == 1) ? "2CH" : "3CH");
	}
	else {
		printf("HwModel = %d (%s)\n", pcal->hwModel, (pcal->hwModel == 1) ? "2CH" : "3CH");
	}
}

static void cmd_debug(char *par) {
  char *p, *next;
	p = get_entry (par, &next);

  if (p != NULL) {
      ntDebugLevel = atoi(p);
		  printf("ntDebugLevel = %d\n",ntDebugLevel );
    
  }
	else {
		  printf("*** not enough input arguments ...\n");
	}
}

// static void cmd_gwEnable(char *par) {
// 	char *p, *next;

// 	p = get_entry (par, &next);
// 	if (p != NULL) {
// 		sscanf(p, "%d", &pdb->comm.gwEable);
// 		printf("gwEnable = %d\n", pdb->comm.gwEable);
    
//     if(pdb->comm.gwEable)
//       pdb->comm.RS485MasterMode = 2;    // gateway
// 		saveSettings(pdb);
//   	saveSettings2(pdb2);
    
// 	}
// 	else {
// 		printf("*** not enough input arguments ...\n");
// 	}
// }


static void cmd_hwVersion(char *par) {
	char *p, *next;

	p = get_entry (par, &next);
	if (p != NULL) {
		{ int _v=0; sscanf(p, "%d", &_v); pcal->hwVer = _v; }  /* hwVer U16 */
		printf("HwVersion = %d\n", pcal->hwVer);
		storeHwSettings(pcal);
	}
	else {
		printf("*** not enough input arguments ...\n");
	}	
}

static void cmd_devInfo(char *par) {
	printf("FW VERSION = %02x.%02x\n", pInfo->fwVer >> 8, pInfo->fwVer & 0xff);
	printf("HW MODE    = %d\n", 0);/*hwModel 미사용*/
	printf("HW VERSION = %d\n", pcal->hwVer);
	printf("HW MACress = %d:%d:%d:%d (%02x:%02x:%02x:%02x)\n", 
			pcal->mac[0], pcal->mac[1], pcal->mac[2], pcal->mac[3],
			pcal->mac[0], pcal->mac[1], pcal->mac[2], pcal->mac[3]);
}


void cmd_meter(char *par) {
	printf("U : %.3f, %.3f, %.3f\n", pmeter->U[0], pmeter->U[1], pmeter->U[2]);
	printf("I : %.3f, %.3f, %.3f\n", pmeter->I[0], pmeter->I[1], pmeter->I[2]);
}

void cmd_saveenergy(char *par) {
	//ENERGY_NVRAM *pEgyNvr = &egyNvr;

	storeEnergy();
	printf("[[[Save Energy Data ...]]]\n");
}


void cmd_savecal(char *par) {
	storeHwSettings(pcal);
	printf("[[[Save HW Info & Cal. Data ...]]]\n");
}

static void cmd_reboot(char *par) {
	reqReboot(0x1234);	// smb_rtu.c에 선언
}

static void cmd_initdb(char *par) {
	pcntl->factReset = 0x1234;
}


/* ADCH <ch> <1=CS-LOW/0=CS-HIGH>
 * ADE9000 Chip Select 핀을 수동으로 직접 제어한다.
 * ch: 0=M0(SSP0), 1=M1(SSP1), 2=M2(SSP1)
 * 1=SELECT(LOW), 0=DESELECT(HIGH) */
static void cmd_adch(char *par)
{
	char *p, *next;
	int ch, state;

	p = get_entry(par, &next);
	if (p == NULL) {
		printf("Usage: ADCH <ch> <1=SELECT/0=DESELECT>\n");
		printf("  ch   : 0=M0(SSP0)  1=M1(SSP1)  2=M2(SSP1)\n");
		printf("  state: 1=CS LOW(선택)  0=CS HIGH(해제)\n");
		return;
	}
	ch = atoi(p);

	p = get_entry(next, &next);
	if (p == NULL) {
		printf("Usage: ADCH <ch> <1=SELECT/0=DESELECT>\n");
		return;
	}
	state = atoi(p);

	if (ch < 0 || ch >= METER_CH_COUNT) {
		printf("[ADCH] Error: ch=%d, valid range 0~%d\n", ch, METER_CH_COUNT - 1);
		return;
	}

	if (state) {
		selectMeter(ch);
		printf("[ADCH] M%d CS --> LOW  (Selected)\n", ch);
	} else {
		deSelectMeter(ch);
		printf("[ADCH] M%d CS --> HIGH (Deselected)\n", ch);
	}
}

/* ADST [ch]
 * ADE9000 채널별 SPI 통신 상태(chipId/version/fail/retry)를 출력한다.
 * ch 생략 시 전체 채널 출력. */
static void cmd_adst(char *par)
{
	char *p, *next;
	int i, start = 0, end = METER_CH_COUNT - 1;
	const ADE_STATUS *st;

	p = get_entry(par, &next);
	if (p != NULL) {
		int ch = atoi(p);
		if (ch < 0 || ch >= METER_CH_COUNT) {
			printf("[ADST] Error: ch=%d, valid range 0~%d\n", ch, METER_CH_COUNT - 1);
			return;
		}
		start = end = ch;
	}

	printf("=== ADE9000 Status ===\n");
	for (i = start; i <= end; i++) {
		st = getAdeStatus(i);
		printf("[M%d] %s | chipId=0x%08x | version=0x%04x | fail=%d | retry=%d\n",
		       i,
		       st->online ? "ONLINE " : "OFFLINE",
		       st->chipId, st->version,
		       st->failCount, st->retryCount);
	}
}

static int tdebug;
uint8_t mdebug;

void tdebug_on(void)
{
 	//tdebug = 1;
 	mdebug=1;
}

void tdebug_off(void)
{
 	tdebug = 0;
 	mdebug = 0;
 }


// static void cmd_iodb(char *par) {
//   pcntl->ioDbFlag = 0x1234;
// }

// static void cmd_doOn(char *par) {
//  char *p, *next;
//  int  pnt=0;
// 	p = get_entry (par, &next);

//   if (p != NULL) {
//       pnt = atoi(p);
// 		  printf("pnt = %d\n",pnt );
    
//   }
// 	else {
// 		  printf("*** not enough input arguments ...\n");
// 	}
//   sendIOCommand(pnt,1);
// }

// static void cmd_doOff(char *par) {
//  char *p, *next;
//  int  pnt=0;
// 	p = get_entry (par, &next);

//   if (p != NULL) {
//       pnt = atoi(p);
// 		  printf("pnt = %d\n",pnt);
    
//   }
// 	else {
// 		  printf("*** not enough input arguments ...\n");
// 	}
//   sendIOCommand(pnt,0);
// }


static void cmd_blup(char *par) {
#if LCD_CONFG
	LcdBackLightUpDown(10);
#endif
}

static void cmd_bldown(char *par) {
#if LCD_CONFG	
	LcdBackLightUpDown(-10);
#endif
}

static void cmd_TrVTrg(char *par) {
	pcntl->TrVTrg = 1;
}

static void cmd_TrCTrg(char *par) {
	pcntl->TrCTrg = 1;
}

static void cmd_ethrst(char *par) {
	Chip_GPIO_SetPinOutLow (LPC_GPIO_PORT, 4, 12);
	osDelayTask(10);
	Chip_GPIO_SetPinOutHigh (LPC_GPIO_PORT, 4, 12);
	printf("---> EthRST ...\n");
}
//
//
//

void init_card (void) {
#ifdef HWV1
  /* RL-FlashFS: SF0_DEF → 기본 드라이브 S: (SPIFI NOR, sf0_drv) */
  {
    U32 retv;

    if (s_fsMounted) {
      return;
    }

    retv = finit (NULL);
    /* 임베디드 NOR(SPIFI)은 카드처럼 "삽입 대기"가 없다. 새/blank 칩은 finit=1(미포맷)을
     *  반환하므로 finit!=0 이면 한 번 포맷 시도 (retv==1 제외하면 새 칩이 영영 포맷 안 됨) */
    if (retv != 0) {
      printf ("\nSPI Flash: finit=%u, try FORMAT\n", (unsigned)retv);
      strcpy (&in_line[0], "KEIL\r\n");
      cmd_format (&in_line[0]);
      retv = finit (NULL);
    }
    if (retv != 0) {
      printf ("\nSPI Flash FS finit failed (%u)\n", (unsigned)retv);
    } else {
      s_fsMounted = 1;
    }
  }
#else
  U32 retv;

  if (s_fsMounted) {
    return;
  }

  while ((retv = finit (NULL)) != 0) {        /* Wait until the Card is ready */
    if (retv == 1) {
      printf ("\nSD/MMC Init Failed");
      printf ("\nInsert Memory card and press key...\n");
    }
    else {
      printf ("\nSD/MMC Card is Unformatted");
      strcpy (&in_line[0], "KEIL\r\n");
      cmd_format (&in_line[0]);
    }
  }
  if (retv == 0) {
    s_fsMounted = 1;
  }
#endif
	
#ifdef USE_FREERTOS	// FreeRTOS에서 finit() 호출 후 SystemCoreClock 값이 0 으로 변경된다.
	SystemCoreClockUpdate();
#endif
}

void ls() {
   cmd_dir(NULL);
}

typedef struct scmd {
  char val[10];
  void (*func)(char *par);
} SCMD;


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
	"SAVEENG", cmd_saveenergy,
	"CLROS", cmd_clrdcos,
	"DCOS",  cmd_dcos,
	"TIME", cmd_datetime,
	"INITQUAL", cmd_initQual,
	"SNSET", cmd_macset,
	"MODEL", cmd_hwModel,
//	"HWMODEL", cmd_hwModel,
	"HWVER", cmd_hwVersion,
//	"GWENABLE", cmd_gwEnable,
	"DEVINFO", cmd_devInfo,
	"INITDB", cmd_initdb,
  "REBOOT", cmd_reboot,
	"BL+", cmd_blup,
	"BL-", cmd_bldown,
	"ETHRST", cmd_ethrst,
	"TRVTRG", cmd_TrVTrg,
	"TRCTRG", cmd_TrCTrg,
	"FDON", tdebug_on,
	"FDOFF", tdebug_off,
	"DEBUG", cmd_debug,
	"ADCH", cmd_adch,
	"ADST", cmd_adst,

//	"FLOWSET", cmd_flowset,
  // "VTHDSET", cmd_v_thdoffset, 
  // "ITHDSET", cmd_i_thdoffset, 
//"IODB", cmd_iodb,
//"DOON", cmd_doOn,
//"DOOFF", cmd_doOff,

#ifdef	FTP_WAVE
	"FTPC", cmd_ftpc
#endif

};

#define CMD_COUNT   (sizeof (cmd) / sizeof (cmd[0]))

void init_directory() {
	FILE *fp;
	uint32_t init=0;
	char path[64];
	
	/* FS 미마운트(finit 실패) 시 fopen이 NULL → fclose(NULL)로 HardFault 나므로 진입 차단 */
	if (!s_fsMounted) {
		printf("{{FS not mounted - skip directory init}}\n");
		return;
	}

	sprintf(path, "init.ini");
	fp = fopen(path, "rb");
	if (fp == NULL) {
		printf("{{Can't open Init File(%s)}}\n", path);

		fp = fopen(CONCAT(SYS_DIR, TEMP_FILE), "wb");
		if (fp) fclose(fp);
		printf("[[Create Directory(%s)]]\n", CONCAT(SYS_DIR, TEMP_FILE));

		fp = fopen(CONCAT(LOG_PQ_DIR, TEMP_FILE), "wb");
		if (fp) fclose(fp);
		printf("[[Create Directory(%s)]]\n", CONCAT(LOG_PQ_DIR, TEMP_FILE));

		fp = fopen(CONCAT(LOG_TREND_DIR, TEMP_FILE), "wb");
		if (fp) fclose(fp);
		printf("[[Create Directory(%s)]]\n", CONCAT(LOG_TREND_DIR, TEMP_FILE));

		fp = fopen(CONCAT(TRG_PQ_DIR, TEMP_FILE), "wb");
		if (fp) fclose(fp);
		printf("[[Create Directory(%s)]]\n", CONCAT(TRG_PQ_DIR, TEMP_FILE));

		fp = fopen(CONCAT(TRG_TRANSIENT_DIR, TEMP_FILE), "wb");
		if (fp) fclose(fp);
		printf("[[Create Directory(%s)]]\n", CONCAT(TRG_TRANSIENT_DIR, TEMP_FILE));

		fp = fopen(CONCAT(FW_DIR, TEMP_FILE), "wb");
		if (fp) fclose(fp);
		printf("[[Create Directory(%s)]]\n", CONCAT(FW_DIR, TEMP_FILE));

		fp = fopen(CONCAT(ALARM_DIR, TEMP_FILE), "wb");
		if (fp) fclose(fp);
		printf("[[Create Directory(%s)]]\n", CONCAT(ALARM_DIR, TEMP_FILE));

		fp = fopen(CONCAT(EVENT_DIR, TEMP_FILE), "wb");
		if (fp) fclose(fp);
		printf("[[Create Directory(%s)]]\n", CONCAT(EVENT_DIR, TEMP_FILE));

		init=1;
		fp = fopen(F_INIT, "wb");
		if (fp) {
			fwrite(&init, sizeof(init), 1, fp);
			fclose(fp);
		}

		printf("[[Create System Directory, init=%d...]]\n", init);
	}
	else {
		fread(&init, sizeof(int), 1, fp);
		fclose(fp);

		printf("[[Init Mode = %d\n", init);
	}
}


void FS_Init() {
	char buf[] = "Hello World";
	FILE *fp;
	printf (intro);                               /* display example info       */
	printf (help);
	init_card ();	
	init_directory();

	// fp = fopen("test1.txt", "w");
	// fwrite(buf, sizeof(buf), 1, fp);
	// fclose(fp);
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

#endif   // FS