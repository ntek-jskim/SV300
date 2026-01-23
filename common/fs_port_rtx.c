/**
 * @file fs_port_rl_fs.c
 * @brief File system abstraction layer (RL-FlashFS)
 *
 * @section License
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2010-2024 Oryx Embedded SARL. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * @author Oryx Embedded SARL (www.oryx-embedded.com)
 * @version 2.4.4
 **/

//Dependencies
#include <string.h>
#include "fs_port.h"
#if 1
   #include "fs_port_rtx.h"
   #include <File_Config.h>
#else
   #include "fs_port_rl_fs.h"
#endif
#include "str.h"
#include "path.h"
#include "error.h"
#include "debug.h"

static FsDir dirTable;


/**
 * @brief File system initialization
 * @return Error code
 **/

__weak_func error_t fsInit(void)
{
   error_t error;
   int status;

   //Initialize file system
   status = finit("M0:");

   //On success, fsOK is returned
   if(status == 0)
   {
      error = NO_ERROR;
   }
   else
   {
      error = ERROR_FAILURE;
   }

   //Return status code
   return error;
}


/**
 * @brief Check whether a file exists
 * @param[in] path NULL-terminated string specifying the filename
 * @return The function returns TRUE if the file exists. Otherwise FALSE is returned
 **/

bool_t fsFileExists(const char_t *path)
{
   int status;
   FINFO fileInfo;
   bool_t found;
	
	printf("%s --> %s\n", __FUNCTION__, path);

   //Initialize flag
   found = FALSE;

   //Make sure the pathname is valid
   if(path != NULL)
   {
      //The fileID field must be initialized to zero
      fileInfo.fileID = 0;
      //Find the specified path name
      status = ffind(path, &fileInfo);

      //Check status code
      if(status == 0 && !(fileInfo.attrib & 0x10))
      {
         found = TRUE;
      }
   }

   //The function returns TRUE if the file exists
   return found;
}


/**
 * @brief Retrieve the size of the specified file
 * @param[in] path NULL-terminated string specifying the filename
 * @param[out] size Size of the file in bytes
 * @return Error code
 **/

error_t fsGetFileSize(const char_t *path, uint32_t *size)
{
   int status;
   FINFO fileInfo;
	
	//printf("%s --> %s\n", __FUNCTION__, path);

   //Check parameters
   if(path == NULL || size == NULL)
      return ERROR_INVALID_PARAMETER;

   //The fileID field must be initialized to zero
   fileInfo.fileID = 0;
   //Find the specified path name
   status = ffind(path, &fileInfo);

   //Any error to report?
   if(status != 0)
      return ERROR_FAILURE;

   //Return the size of the file
   *size = fileInfo.size;

   //Successful processing
   return NO_ERROR;
}


/**
 * @brief Retrieve the attributes of the specified file
 * @param[in] path NULL-terminated string specifying the filename
 * @param[out] fileStat File attributes
 * @return Error code
 **/

error_t fsGetFileStat(const char_t *path, FsFileStat *fileStat)
{
   int status;
   FINFO fileInfo;
	
	//printf("%s --> %s\n", __FUNCTION__, path);

   //Check parameters
   if(path == NULL || fileStat == NULL)
      return ERROR_INVALID_PARAMETER;

   //The fileID field must be initialized to zero
   fileInfo.fileID = 0;
   //Find the specified path name
   status = ffind(path, &fileInfo);

   //Any error to report?
   if(status != 0)
      return ERROR_FAILURE;

   //Clear file attributes
   osMemset(fileStat, 0, sizeof(FsFileStat));

   //File attributes
   fileStat->attributes = fileInfo.attrib;
   //File size
   fileStat->size = fileInfo.size;

   //Time of last modification
   fileStat->modified.year = fileInfo.time.year;
   fileStat->modified.month = fileInfo.time.mon;
   fileStat->modified.day = fileInfo.time.day;
   fileStat->modified.hours = fileInfo.time.hr;
   fileStat->modified.minutes = fileInfo.time.min;
   fileStat->modified.seconds = fileInfo.time.sec;
   fileStat->modified.milliseconds = 0;

   //Make sure the date is valid
   fileStat->modified.month = MAX(fileStat->modified.month, 1);
   fileStat->modified.month = MIN(fileStat->modified.month, 12);
   fileStat->modified.day = MAX(fileStat->modified.day, 1);
   fileStat->modified.day = MIN(fileStat->modified.day, 31);

   //Successful processing
   return NO_ERROR;
}


/**
 * @brief Rename the specified file
 * @param[in] oldPath NULL-terminated string specifying the pathname of the file to be renamed
 * @param[in] newPath NULL-terminated string specifying the new filename
 * @return Error code
 **/

error_t fsRenameFile(const char_t *oldPath, const char_t *newPath)
{
   error_t error;
   int status;
   const char_t *newName;

   //Check parameters
   if(oldPath == NULL || newPath == NULL)
      return ERROR_INVALID_PARAMETER;

   //Rename the specified file
   status = frename(oldPath, newPath);

   //On success, fsOK is returned
   if(status == 0)
   {
      error = NO_ERROR;
   }
   else
   {
      error = ERROR_FAILURE;
   }

   //Return status code
   return error;
}


/**
 * @brief Delete a file
 * @param[in] path NULL-terminated string specifying the filename
 * @return Error code
 **/

error_t fsDeleteFile(const char_t *path)
{
   error_t error;
   int status;
	char *p = (char *)path;
	
   // '/' => '\\'로 변환한다
   while (*p != NULL) {
      if(*p == '/') *p = '\\';
      p++;
   }
	
	printf("%s --> %s\n", __FUNCTION__, path);
	
   //Make sure the pathname is valid
   if(path == NULL)
      return ERROR_INVALID_PARAMETER;

   //Delete the specified file
   status = fdelete(path);

   //On success, fsOK is returned
   if(status == 0)
   {
      error = NO_ERROR;
   }
   else
   {
      error = ERROR_FAILURE;
   }

   //Return status code
   return error;
}


/**
 * @brief Open the specified file for reading or writing
 * @param[in] path NULL-terminated string specifying the filename
 * @param[in] mode Type of access permitted (FS_FILE_MODE_READ,
 *   FS_FILE_MODE_WRITE or FS_FILE_MODE_CREATE)
 * @return File handle
 **/

FsFile *fsOpenFile(const char_t *path, uint_t mode)
{
   char_t s[4];
   char *p = (char *)path;
   //File pointer
   FILE *fp = NULL;
	
	printf("%s --> %s\n", __FUNCTION__, path);
   
   // '/' => '\\'로 변환한다
   while (*p != NULL) {
      if(*p == '/') *p = '\\';
      p++;
   }

   //Make sure the pathname is valid
   if(path == NULL)
      return NULL;

   //Check file access mode
   if(mode & FS_FILE_MODE_WRITE)
   {
      osStrcpy(s, "wb");
   }
   else
   {
      osStrcpy(s, "rb");
   }

   //Open the specified file
   fp = fopen(path, s);

   //Return a handle to the file
   return fp;
}


/**
 * @brief Move to specified position in file
 * @param[in] file Handle that identifies the file
 * @param[in] offset Number of bytes to move from origin
 * @param[in] origin Position used as reference for the offset (FS_SEEK_SET,
 *   FS_SEEK_CUR or FS_SEEK_END)
 * @return Error code
 **/

error_t fsSeekFile(FsFile *file, int_t offset, uint_t origin)
{
   error_t error;
   int_t ret;

   //Make sure the file pointer is valid
   if(file == NULL)
      return ERROR_INVALID_PARAMETER;

   //The origin is used as reference for the offset
   if(origin == FS_SEEK_CUR)
   {
      //The offset is relative to the current file pointer
      origin = SEEK_CUR;
   }
   else if(origin == FS_SEEK_END)
   {
      //The offset is relative to the end of the file
      origin = SEEK_END;
   }
   else
   {
      //The offset is absolute
      origin = SEEK_SET;
   }

   //Move read/write pointer
   ret = fseek(file, offset, origin);

   //On success, zero is returned
   if(ret == 0)
   {
      error = NO_ERROR;
   }
   else
   {
      error = ERROR_FAILURE;
   }

   //Return status code
   return error;
}


/**
 * @brief Write data to the specified file
 * @param[in] file Handle that identifies the file to be written
 * @param[in] data Pointer to a buffer containing the data to be written
 * @param[in] length Number of data bytes to write
 * @return Error code
 **/

error_t fsWriteFile(FsFile *file, void *data, size_t length)
{
   error_t error;
   int_t n;

   //Make sure the file pointer is valid
   if(file == NULL)
      return ERROR_INVALID_PARAMETER;

   //Write data
   n = fwrite(data, sizeof(uint8_t), length, file);

   //The total number of elements successfully written is returned. If this
   //number differs from the count parameter, a writing error prevented the
   //function from completing
   if(n == length)
   {
      error = NO_ERROR;
   }
   else
   {
      error = ERROR_FAILURE;
   }

   //Return status code
   return error;
}


/**
 * @brief Read data from the specified file
 * @param[in] file Handle that identifies the file to be read
 * @param[in] data Pointer to the buffer where to copy the data
 * @param[in] size Size of the buffer, in bytes
 * @param[out] length Number of data bytes that have been read
 * @return Error code
 **/

error_t fsReadFile(FsFile *file, void *data, size_t size, size_t *length)
{
   error_t error;
   int_t n;

   //Check parameters
   if(file == NULL || length == NULL)
      return ERROR_INVALID_PARAMETER;

   //No data has been read yet
   *length = 0;

   //Read data
   n = fread(data, sizeof(uint8_t), size, file);

   //The total number of elements successfully read is returned. If this
   //number differs from the count parameter, either a reading error occurred
   //or the end-of-file was reached while reading
   if(n != 0)
   {
      //Total number of data that have been read
      *length = n;

      //Successful processing
      error = NO_ERROR;
   }
   else
   {
      //Report an error
      error = ERROR_END_OF_FILE;
   }

   //Return status code
   return error;
}


/**
 * @brief Close a file
 * @param[in] file Handle that identifies the file to be closed
 **/

void fsCloseFile(FsFile *file)
{
   //Make sure the file pointer is valid
   if(file != NULL)
   {
      //Close the specified file
      fclose(file);
   }
}


/**
 * @brief Check whether a directory exists
 * @param[in] path NULL-terminated string specifying the directory path
 * @return The function returns TRUE if the directory exists. Otherwise FALSE is returned
 **/

bool_t fsDirExists(const char_t *path)
{
   int status;
   FINFO fileInfo;
   bool_t found;
	char *p = (char *)path;
	
   //Initialize flag
   found = FALSE;
	
	printf("%s --> %s\n", __FUNCTION__, path);
	
	// 2025-2-21, '/' => '\\'로 변환한다
   while (*p != NULL) {
      if(*p == '/') *p = '\\';
      p++;
   }

   //Make sure the pathname is valid
   if(path != NULL)
   {
      //Root directory?
      if(osStrcmp(path, "/") == 0 || osStrcmp(path, "\\") == 0)
      {
         //The root directory always exists
         found = TRUE;
      }
      else
      {
         //The fileID field must be initialized to zero
         fileInfo.fileID = 0;
         //Find the specified path name
         status = ffind(path, &fileInfo);

         //Check status code
         if(status == 0)
         {            
            //Valid directory?
            if((fileInfo.attrib & 0x10) != 0)
            {
               found = TRUE;
            }
         }
      }
   }

   //The function returns TRUE if the directory exists
   return found;
}


/**
 * @brief Create a directory
 * @param[in] path NULL-terminated string specifying the directory path
 * @return Error code
 **/

error_t fsCreateDir(const char_t *path)
{
   error_t error;
   int status;

   error = ERROR_FAILURE;

   //Return status code
   return error;
}


/**
 * @brief Remove a directory
 * @param[in] path NULL-terminated string specifying the directory path
 * @return Error code
 **/

error_t fsRemoveDir(const char_t *path)
{
   error_t error;
   int status;

   error = ERROR_FAILURE;

   //Return status code
   return error;
}


/**
 * @brief Open a directory stream
 * @param[in] path NULL-terminated string specifying the directory path
 * @return Directory handle
 **/

FsDir *fsOpenDir(const char_t *path)
{
   FsDir *dir = &dirTable;
   FINFO info;
   int nitem=0;
   char *p = (char *)path;
	
	printf("%s --> %s\n", __FUNCTION__, path);
   
   // '/' => '\\'로 변환한다
   while (*p != NULL) {
      if(*p == '/') *p = '\\';
      p++;
   }

   // path를 dir->path에 저장한다
   if(path == NULL || strcmp(path, "\\") == 0) {
      strcpy(dir->path, "*.*");
   }
   else {
      strcpy(dir->path, path);
      // 2025-2-21, RTX FS: dir 목록 요청시 DirName 뒤에  "\\*.*"가  추가한다
		strcat(dir->path, "\\*.*");
   }

   // OpenDir에서는 dir에 포함된 정보를 한번에 하나만 읽는다
   // 남은 정보는  ReadDir에서 읽는다
   dir->eof = 0;
   // directory명으로 ffind를 한 후 결과를 dir에 저장한다
   dir->info.fileID  = 0;
   if (ffind (dir->path, &dir->info) == 0) {
      return &dirTable;
   }
   else {
      return NULL;
   }
}


/**
 * @brief Read an entry from the specified directory stream
 * @param[in] dir Handle that identifies the directory
 * @param[out] dirEntry Pointer to a directory entry
 * @return Error code
 **/

error_t fsReadDir(FsDir *dir, FsDirEntry *dirEntry)
{
   error_t error;
   int status;
   FINFO fileInfo;

   //Check parameters
   if(dir == NULL || dirEntry == NULL)
      return ERROR_INVALID_PARAMETER;

   //Clear directory entry
   osMemset(dirEntry, 0, sizeof(FsDirEntry));

	//printf("%s --> %s\n", __FUNCTION__, dir->info.name);
	
   // OpenDir에서 읽는 정보를 표시하고, 남은 파일정보를 하나 읽고  dir에 저장한다
   //Valid directory entry?
   //Copy the file name component
   strSafeCopy(dirEntry->name, (char *)dir->info.name, FS_MAX_NAME_LEN);

   //File attributes
   dirEntry->attributes = dir->info.attrib;
   //File size
   dirEntry->size = dir->info.size;

   //Time of last modification
   dirEntry->modified.year = dir->info.time.year;
   dirEntry->modified.month = dir->info.time.mon;
   dirEntry->modified.day = dir->info.time.day;
   dirEntry->modified.hours = dir->info.time.hr;
   dirEntry->modified.minutes = dir->info.time.min;
   dirEntry->modified.seconds = dir->info.time.sec;
   dirEntry->modified.milliseconds = 0;

   //Make sure the date is valid
   dirEntry->modified.month = MAX(dirEntry->modified.month, 1);
   dirEntry->modified.month = MIN(dirEntry->modified.month, 12);
   dirEntry->modified.day = MAX(dirEntry->modified.day, 1);
   dirEntry->modified.day = MIN(dirEntry->modified.day, 31);

   if (ffind(dir->path, &dir->info) == 0)
   {
      //Successful processing
      error = NO_ERROR;
   }
   else
   {
      if (dir->eof == 0) {
         dir->eof = 1;
         error = NO_ERROR;         
      }
      else {
         //End of the directory stream
         error = ERROR_END_OF_STREAM;
      }
   }

   //Return status code
   return error;
}


/**
 * @brief Close a directory stream
 * @param[in] dir Handle that identifies the directory to be closed
 **/

void fsCloseDir(FsDir *dir)
{
   //Make sure the directory pointer is valid
   if(dir != NULL)
   {
      //Release directory descriptor
      osFreeMem(dir);
   }
}
