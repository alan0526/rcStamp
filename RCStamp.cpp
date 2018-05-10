// RCStamp.cpp : Defines the entry point for the console application.
//
/* My mods:
 *
 * Oct 2/12   Converted to MSVC 2010 & added code to get its
 *            own version in.
 *            Added error message if target file not found.
 * Jan 12/09  made keeping file time optional with -t option
 *            it caused a lot of trouble since it made
 *            the make process skip the resource compile 
 *            except for a complete rebuild
 * Aug 4/07   added mod to save .rc file write time to avoid
 *            recompile due to updates to version in .rc 
 *            when the rest of the source has not changed
 *            Left code in main program to avoid dealing with
 *            all of the possible return paths in ProcessFile()
 * 
 * Aug 3/07 - inital version on my systems
 *            added mod to fix etra CR/LF at end of .rc file
 *
 * Note: current debug code uses the work file stamp.rc in the 
 * main directory for debugging purposes
 */
#include "stdafx.h"  

#include <iostream>
#include <fstream>
#include <strstream>
#include <string>
#include <windows.h>
#include <stdio.h>
#include <direct.h>

//#include "version.info"

using namespace std;

char const * strUsage1 =
" RCSTAMP  - ";
char const * strUsage2 =
"     command line:\r\n"
"  rcstamp file <g_format> [options...]\r\n"
"  rcstamp @file [<g_format>] [options...]\r\n"
"\r\n"
"  file  : resource script to modify\r\n"
"  @file : file containing a list of file(s) (see below)\r\n"
"\r\n"
"  g_format: specifies version number changes, e.g. *.*.33.+\r\n"
"          * : don't modify position\r\n"
"          + : increment position by one\r\n"
"          number : set position to this value\r\n"
"  \r\n"
"  options:    \r\n"
"    -n  :         don't update the FileVersion string \r\n"
"                  (default: set to match the FILEVERSION value)\r\n"
"    -rRESNAME:    update only version resource with resource id g_resName\r\n"
"                  (default: update all Version resources)\r\n"
"    -l            the specified file(s) are file list files\r\n"
"    -v            Verbose\r\n"
"    -t            do not change file time\r\n"
"  file list files:\r\n"
"  can specify one file on each line, e.g.\r\n"
"  d:\\sources\\project1\\project1.rc\r\n"
"  d:\\sources\\project2\\project2.rc=*.*.*.+\r\n"
"\r\n"
"  a g_format in the list file overrides the g_format from the command line\r\n"
"  using the -l option, list files themselves can be modified\r\n"
"\r\n";


// command line options:
//
//  rcstamp file <g_format> [options...]
//  rcstamp @file [<g_format>] [options...]
//
//  file  : resource script to modify
//  @file : file containing a list of file (see below)
//
//  
//  g_format: specifies verison numbr changes, e.g. *.*.33.+
//          * : don't modify
//          + : increment by one
//          number : set to this value
//          P : take from product version
//  
//  options:    
//    -n  :         don't update the FileVersion string 
//                  (by default, set to match the FILEVERSION value)
//      
//    -rRESNAME:    replace version resource with resource id g_resName
//                  (default: replace 
//
//    -l            the specified file (or files in the file list) are file list files
//
//    -v            Verbose
//    
//  file list files:
//
//  can specify one file on each line, e.g.
//  d:\sources\project1\project1.rc
//  d:\sources\project2\project2.rc=*.*.*.+
//
//  specifying a g_format overrides the g_format specified on the command line
//  using the -l option, list files itself can be modified
//
//


// Command Line Information

char const *    scriptFile      = NULL;         // single resource script (.rc), or name of list file
bool            scriptFileList  = false;        // script file is a file list file
char const *    g_format        = NULL;         // g_format specifier
char const *    g_resName       = NULL;

bool            processListFile = false;        // the file(s) specified are list files, not resource files
bool            noReplaceString = false;        // don't replace fileversion string
bool            verbose         = false;
bool            keepFileTime    = false;        // keep file time unaltered


bool ParseArg(char const * arg)
{
  if (*arg!='-' && *arg!='/') 
  {
    if (scriptFile == NULL) 
    {
      scriptFile = arg;
      if (*scriptFile=='@') 
      {
        scriptFileList = true;
        ++scriptFile;
      }
    }
    else if (g_format == NULL) 
    {
      g_format = arg;
    }
    else {
      cerr << "Unexpected argument\"" << arg << "\"\r\n";
      return false;
    }
    return true;
  }

  ++arg;
  char c = tolower(*arg);

  if (c=='n')
    noReplaceString = true;
  else if (c=='r')
    g_resName = arg+1;
  else if (c=='l')
    processListFile = true;
  else if (c=='v')
    verbose = true;
  else if (c=='t')
    keepFileTime = true;
  else {
    cerr << "Unknown option\"" << arg << "\"\r\n";
    return false;
  }
  return true;
}

// ------------------------------------------------------------------

bool CalcNewVersion(char const * oldVersion, char const * fmtstr, char * version)
{
  if (!fmtstr)
    fmtstr = g_format;
  if( strlen( oldVersion ) == 0 )
  {
    cerr << "Zero length old version\r\n";
    return false;
  }
  char const * fmt[4];
  char * fmtDup = strdup(fmtstr);
  fmt[0] = strtok(fmtDup, " .,");
  fmt[1] = strtok(NULL,   " .,");
  fmt[2] = strtok(NULL,   " .,");
  fmt[3] = strtok(NULL,   " .,");

  if ( (fmt[0] == 0) || (fmt[1] == 0) || (fmt[2] == 0) || (fmt[3] == 0) )
  {
    cerr << "Invalid format\r\n";
    return false;
  }

  char *       outHead = version;

  char * verDup = strdup(oldVersion);
  char * verStr = strtok(verDup, " ,");

  *version = 0;

  for(int i=0; i<4; ++i) 
  {
    int oldVersion = atoi(verStr);
    int newVersion = oldVersion;

    char c = fmt[i][0];

    if (strcmp(fmt[i], "*")==0)
       newVersion = oldVersion;
    else if (isdigit(c))
      newVersion = atoi(fmt[i]);
    else if (c=='+' || c=='-') {
      if (isdigit(fmt[i][1]))
         newVersion = oldVersion + atoi(fmt[i]);
      else
         newVersion = oldVersion + ((c=='+') ? 1 : -1);
    }
    itoa(newVersion, outHead, 10);
    outHead += strlen(outHead);

    if (i != 3) 
    {
      strcpy(outHead, ", ");
      outHead += 2;
      verStr = strtok(NULL, " ,");
    }
  }
  free(fmtDup);
  free(verDup);
  return true;
}

// ------------------------------------------------------------------

bool ProcessFile(char const * fileName, char const * fmt = NULL)
{
  const int MAXLINELEN = 2048;
  ifstream is(fileName);
  if (is.fail()) {
      cerr << "cannot open " << fileName << "\r\n";
      return false;
  }
  string result;

  char line[MAXLINELEN];
  char version[64] = { 0 };       // "final" version string
  bool inReplaceBlock = false;
    
  while (!is.eof()) 
  {
    is.getline(line, MAXLINELEN);
    if (is.bad()) {
      cerr << "Error reading " << fileName << "\r\n";
      return false;
    }
    if (processListFile) 
    {
      char * pos = strchr(line, '=');
      if (pos) 
      { 
         CalcNewVersion(pos+1, fmt, pos+1);  // in-place replace
      }
    }
    else 
    {
      char * dupl = strdup(line);
      char * word1 = strtok(dupl, " \t");
      char * word2 = strtok(NULL, " \t,");  // allow comma for [VALUE "FileVersion",] entry

      if (word1 && word2 && strcmpi(word2, "VERSIONINFO") == 0 && strnicmp(word1, "//",2)!=0) 
      {
        if (g_resName==NULL || strcmpi(g_resName, word1)==0) 
          inReplaceBlock = true;
        else
          inReplaceBlock = false;
      }

      if (inReplaceBlock) 
      { 
        if (word1 && strcmpi(word1, "FILEVERSION") == 0) 
        {
          int offset = word1-dupl + strlen("FILEVERSION") + 1;
          CalcNewVersion(line+offset, fmt, version);
          strcpy(line+offset, version);
          if (verbose)
              cout << line << "\r\n"; 
        }
#if 1
        if (word1 && strcmpi(word1, "PRODUCTVERSION") == 0) 
        {
          int offset = word1-dupl + strlen("PRODUCTVERSION") + 1;
//          CalcNewVersion(line+offset, fmt, version);
          strcpy(line+offset, version);
          if (verbose)
              cout << line << "\r\n"; 
        }
#endif
        if (!noReplaceString && word1 && word2 && 
              strcmpi(word2, "\"FileVersion\"")==0 && strcmpi(word1, "VALUE")==0) 
        {
          if (!*version) 
          {
            cerr << "Error: VALUE \"FileVersion\" encountered before FILEVERSION\r\n";
          }
          else 
          {
            char * output = strchr(line, ',');
            if (!output) 
              output = line + strlen(line);
            else 
                ++output;
            sprintf(output, " \"%s\"", version);
            if (verbose)
               cout << line << "\r\n";
          }
        }
#if 1
        if (!noReplaceString && word1 && word2 && 
              strcmpi(word2, "\"ProductVersion\"")==0 && strcmpi(word1, "VALUE")==0) 
        {
          if (!*version) 
          {
            cerr << "Error: VALUE \"ProductVersion\" encountered before PRODUCTVERSION\r\n";
          }
          else 
          {
            char * output = strchr(line, ',');
            if (!output) 
              output = line + strlen(line);
            else 
                ++output;
            sprintf(output, " \"%s\"", version);
            if (verbose)
               cout << line << "\r\n";
          }
        }
#endif
      }
      free(dupl); 
    }
    result += line;
    // fix addition of extra blank lines at end
    if( ! is.eof() )
      result += "\n";
  }
  // re-write file
  is.close();  

  ofstream os(fileName);
  if (os.fail()) 
  {
    cerr << "Cannot write " << fileName << "\r\n";
    return false;
  }
  os.write( &result[0], result.size());

  if (os.fail()) {
    cerr << "Error writing " << fileName << "\r\n";
    return false;
  }
  os.close();
  return true;
}

// ------------------------------------------------------------------

void GetFileVersionOfApplication( WORD *pMajorVersion,
  WORD *pMinorVersion, WORD *pBuildNumber )
{
  HMODULE hModule = GetModuleHandleW(NULL);
  char path[MAX_PATH];
  GetModuleFileName( hModule, path, MAX_PATH );
  LPTSTR lpszFilePath =path;

  // from: http://vcpptips.wordpress.com/tag/verqueryvalue/
  WORD RevisionNumber; 
  DWORD dwHandle, dwLen; 
  UINT BufLen; 
  LPTSTR lpData; 
  VS_FIXEDFILEINFO *pFileInfo; 
  dwLen = GetFileVersionInfoSize( lpszFilePath, &dwHandle ); 
  if (!dwLen)   
    return;// FALSE; 
  lpData = (LPTSTR) malloc (dwLen); 
  if (!lpData)   
    return;// FALSE; 
  if( !GetFileVersionInfo( lpszFilePath, dwHandle, dwLen, lpData ) ) 
  {  
    free (lpData);  
    return;// FALSE; 
  } 
  if( VerQueryValue( lpData, "\\", (LPVOID *) &pFileInfo, (PUINT)&BufLen ) )  
  {  
    *pMajorVersion = HIWORD(pFileInfo->dwFileVersionMS);  
    *pMinorVersion = LOWORD(pFileInfo->dwFileVersionMS);  
    *pBuildNumber = HIWORD(pFileInfo->dwFileVersionLS);  
    RevisionNumber = LOWORD(pFileInfo->dwFileVersionLS);  
    free (lpData);  
    return;// TRUE; 
  } 
  free (lpData);
}

// ------------------------------------------------------------------

int main(int argc, char* argv[])
{
  WORD wMajorVersion;
  WORD wMinorVersion;
  WORD wBuildNumber;

  GetFileVersionOfApplication( &wMajorVersion, &wMinorVersion, &wBuildNumber);
  if (argc == 1) 
  {
    cerr << strUsage1;
    cerr << "  Version: " << wMajorVersion << '.' << wMinorVersion;
    cerr << " - Build: " << wBuildNumber << "\r\n";
    cerr << strUsage2;
    return 3;
  }
  for(int i=1; i<argc; ++i) 
  {
    if (!ParseArg(argv[i]))
      return 3;
  }

  if (!scriptFile || (!scriptFileList && !g_format)) 
  {
    cerr << "No formatting options specified\r\n";
    return 3;
  }
  if (verbose)
  {
    cerr << strUsage1;
    cerr << "  Version: " << wMajorVersion << '.' << wMinorVersion;
    cerr << " - Build: " << wBuildNumber << "\r\n";
  }
    
  bool errorOccured = false;

  if (scriptFileList) 
  {
      const int MAXLINELEN = 2048;
      ifstream is(scriptFile);
      char line[MAXLINELEN];

      while (!is.bad() && !is.eof()) 
      {
          is.getline(line, MAXLINELEN);

          if (*line==0 || *line == ';')
              continue;

          if (verbose)
              cout << line << "\r\n";

          char * eqpos = strchr(line, '=');
          char const * fmt = g_format;

          if (eqpos) {        // '=' found
              *eqpos = 0;     // Null-terminate the file name
              fmt = eqpos + 1;
          }
          HANDLE hFile = CreateFile( line, 
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, NULL
          );
          if( hFile == INVALID_HANDLE_VALUE )
          {
            errorOccured = true;
            continue;
          }
          // else proceed
          FILETIME ftCreate, ftAccess, ftWrite;
          if( ! GetFileTime( hFile, &ftCreate, &ftAccess,
              &ftWrite ) )
          {
            errorOccured = true;
            CloseHandle( hFile );
            continue;
          }
          CloseHandle( hFile );
          ProcessFile(line, fmt);
          hFile = CreateFile( line, 
            GENERIC_READ | GENERIC_WRITE,
            0, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, NULL
          );
          if( hFile == INVALID_HANDLE_VALUE )
          {
            return FALSE;
          }
          // else proceed
          if( ! SetFileTime( hFile, &ftCreate, &ftAccess,
            &ftWrite ) )
          {
            CloseHandle( hFile );
            return FALSE;
          }
          CloseHandle( hFile );
      }
  }
  else 
  {
    HANDLE hFile = CreateFile( scriptFile, 
    GENERIC_READ | GENERIC_WRITE,
    FILE_SHARE_READ, NULL, OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL, NULL
    );
    if( hFile == INVALID_HANDLE_VALUE )
    {
      cerr << "Cannot create the target file: " << scriptFile <<" \r\n";
      return FALSE;
    }
    // else proceed
    FILETIME ftCreate, ftAccess, ftWrite;
    if( ! GetFileTime( hFile, &ftCreate, &ftAccess,
      &ftWrite ) )
    {
      CloseHandle( hFile );
      return FALSE;
    }
    CloseHandle( hFile );
    errorOccured = ProcessFile(scriptFile, NULL);
    hFile = CreateFile( scriptFile, 
      GENERIC_READ | GENERIC_WRITE,
      0, NULL, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL, NULL
      );
    if( hFile == INVALID_HANDLE_VALUE )
    {
      return FALSE;
    }
    // else proceed
    if( keepFileTime )  // do we want to keep the file time??
    {
      if( ! SetFileTime( hFile, &ftCreate, &ftAccess,
        &ftWrite ) )
      {
        CloseHandle( hFile );
        return FALSE;
      }
      CloseHandle( hFile );
    }
  }
  return 0;
}

// ------------------------------- eof ------------------------------
