# rcStamp
Update Build and Version in Windows RC file
The project was initially taken from the CodeProject article at
https://www.codeproject.com/Articles/2180/RCStamp-add-build-counts-and-more-to-your-rc-files
and then adapted and extended to fill my own needs.

The one change most important for myself was the -t option to preserve the file date and time
to avoid unnecessary rebuilds, though I am not sure it actually does that with the current
build system. Just the same it is still very useful in the pre-build events for my MSVC
projects.

Usage:

 RCSTAMP  -   Version: 1.0 - Build: 78
 
     command line:
	 
  rcstamp file <g_format> [options...]
  
  rcstamp @file [<g_format>] [options...]
  

  file  : resource script to modify
  
  @file : file containing a list of file(s) (see below)
  

  g_format: specifies version number changes, e.g. *.*.33.+
  
          * : don't modify position
		  
          + : increment position by one
		  
          number : set position to this value
		  

  options:
  
    -n  :         don't update the FileVersion string
	
                  (default: set to match the FILEVERSION value)
				  
    -rRESNAME:    update only version resource with resource id g_resName
	
                  (default: update all Version resources)
				  
    -l            the specified file(s) are file list files
	
    -v            Verbose
	
    -t            do not change file time
	
  file list files:
  
  can specify one file on each line, e.g.
  
  d:\sources\project1\project1.rc
  
  d:\sources\project2\project2.rc=*.*.*.+

  a g_format in the list file overrides the g_format from the command line
  using the -l option, list files themselves can be modified

