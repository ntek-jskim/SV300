/*-----------------------------------------------------------------------------
 *      RL-ARM - USB
 *-----------------------------------------------------------------------------
 *      Name:    RETARGET.C 
 *      Purpose: Retarget low level functions
 *-----------------------------------------------------------------------------
 *      This code is part of the RealView Run-Time Library.
 *      Copyright (c) 2004-2013 KEIL - An ARM Company. All rights reserved.
 *----------------------------------------------------------------------------*/

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <rt_sys.h>
#include <File_Config.h>

#pragma import(__use_no_semihosting_swi)

/* The following macro definitions may be used to translate this file:

  STDIO - use standard Input/Output device
          (default is NOT used)
 */

/* Standard IO device handles. */
#define STDIN   0x8001
#define STDOUT  0x8002
#define STDERR  0x8003

/* Standard IO device name defines. */
const char __stdin_name[]  = "STDIN";
const char __stdout_name[] = "STDOUT";
const char __stderr_name[] = "STDERR";

struct __FILE { int handle; /* Add whatever you need here */ };

#define	STDIO // File IO 사용시

#ifdef STDIO

extern int  debug_getchar();
extern void debug_putchar(int c);

/*-----------------------------------------------------------------------------
  Write character to the Serial Port
 *----------------------------------------------------------------------------*/
int writechar(int c) 
{
	if (c == '\n') debug_putchar('\r');
	debug_putchar(c);
  return (c);
}

/*-----------------------------------------------------------------------------
  Read character from the Serial Port
 *----------------------------------------------------------------------------*/
int readchar(void) {	
   int ch = debug_getchar();

   if (ch < 0) 
      return 0;
   else 
      return ch;
}

#endif   // STDIO

/*--------------------------- _ttywrch ---------------------------------------*/

void _ttywrch (int ch) {
#ifdef STDIO
   writechar(ch);
#endif
}

/*--------------------------- _sys_open --------------------------------------*/

FILEHANDLE _sys_open (const char *name, int openmode) {
  /* Register standard Input Output devices. */
  if (strcmp(name, "STDIN") == 0) {
    return (STDIN);
  }
  if (strcmp(name, "STDOUT") == 0) {
    return (STDOUT);
  }
  if (strcmp(name, "STDERR") == 0) {
    return (STDERR);
  }
  return (__sys_open (name, openmode));
}

/*--------------------------- _sys_close -------------------------------------*/

int _sys_close (FILEHANDLE fh) {
  if (fh > 0x8000) {
    return (0);
  }
  return (__sys_close (fh));
}

/*--------------------------- _sys_write -------------------------------------*/

int _sys_write (FILEHANDLE fh, const U8 *buf, U32 len, int mode) 
{
#ifdef STDIO
   if (fh == STDOUT || fh == STDERR) {
      /* Standard Output device. */
      for (  ; len; len--) {
         writechar(*buf++);
      }
      return (0);
   }
#endif
   if (fh > 0x8000) {
      return (-1);
   }
   return (__sys_write (fh, buf, len));
}

/*--------------------------- _sys_read --------------------------------------*/

int _sys_read (FILEHANDLE fh, U8 *buf, U32 len, int mode) 
{
#ifdef STDIO
   if (fh == STDIN) {
      /* Standard Input device. */
      for (  ; len; len--) {
         *buf++ = readchar();
      }
      return (0);
   }
#endif
   if (fh > 0x8000) {
      return (-1);
   }
   return (__sys_read (fh, buf, len));
}

/*--------------------------- _sys_istty -------------------------------------*/

int _sys_istty (FILEHANDLE fh) 
{
   if (fh > 0x8000) {
      return (1);
   }
   return (0);
}

/*--------------------------- _sys_seek --------------------------------------*/

int _sys_seek (FILEHANDLE fh, long pos) 
{
   if (fh > 0x8000) {
      return (-1);
   }
   return (__sys_seek (fh, pos));
}

/*--------------------------- _sys_ensure ------------------------------------*/

int _sys_ensure (FILEHANDLE fh) 
{
   if (fh > 0x8000) {
      return (-1);
   }
   return (__sys_ensure (fh));
}

/*--------------------------- _sys_flen --------------------------------------*/

long _sys_flen (FILEHANDLE fh) 
{
   if (fh > 0x8000) {
      return (0);
   }
   return (__sys_flen (fh));
}

/*--------------------------- _sys_tmpnam ------------------------------------*/

int _sys_tmpnam (char *name, int sig, unsigned maxlen) 
{
   return (1);
}

/*--------------------------- _sys_command_string ----------------------------*/

char *_sys_command_string (char *cmd, int len) 
{
   return (cmd);
}

/*--------------------------- _sys_exit --------------------------------------*/

void _sys_exit (int return_code) {
   /* Endless loop. */
   while (1);
}

/*-----------------------------------------------------------------------------
 * end of file
 *----------------------------------------------------------------------------*/

//void dprintf(const  char *format, ...) {
//	static  char  buffer[256];
//	va_list     args;
//	char	*tmp = buffer;

//	va_start(args, format);
//	vsprintf((char *)buffer, (char const *)format, args);
//	va_end(args);

//	while (*tmp != 0) {
//			sendchar(*tmp++);
//	}
//}