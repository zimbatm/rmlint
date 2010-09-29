/**
*  This file is part of autovac.
*
*  autovac is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  autovac is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with autovac.  If not, see <http://www.gnu.org/licenses/>.
*
** Author: Christopher Pahl <sahib@online.de>:
** Hosted at the time of writing (Mo 30. Aug 14:02:22 CEST 2010): 
*  http://github.com/sahib/autovac
*   
**/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <getopt.h>
#include <dirent.h>
#include <setjmp.h>

#include "rmlint.h"
#include "mode.h" 
#include "filter.h"
#include "defs.h"
#include "mt.h"

/**
 * ToDo: 
 * -man page / README / updated help 
 * - some comments.. clean up..
 * - gettext, to translate msgs   
 * - other hash algorithm.. (sha1? Speed?)
 * - extended ls output --skipped. take -m list -c "ls -lah \"%s\" # == %s" -- bash is so much better than me here.   
 **/

/** 
 * ToDo2 (for removing other sort of "lint") 
 * - Write bad symlinks to script 
 * - Find old tmp data (*~)  
 * - empty dirs 
 * - non stripped binaries 
 * => Should be complete after implementing this. 
 **/ 

bool do_exit = false; 
bool use_cwd = false; 
char *mode_string = NULL; 
int  cpindex = 0; 

/* If we abort abnormally we'd like to set the color back */
static void resetcol(void) { printf(NCO); }
jmp_buf place;

/** Messaging **/ 
void error(const char* format, ...)
{
	if(set.verbosity > 0)
	{
	  va_list args;
	  va_start (args, format);
	  vfprintf (stderr, format, args);
	  va_end (args);
	}
}

void warning(const char* format, ...)
{
	if(set.verbosity > 1)
	{
	  va_list args;
	  va_start (args, format);
	  vfprintf (stderr, format, args);
	  va_end (args);
	}
}

void info(const char* format, ...)
{
	if(set.verbosity > 2)
	{
	  va_list args;
	  va_start (args, format);
	  vfprintf (stdout, format, args);
	  va_end (args);
	}
}

static void print_help(void)
{
	fprintf(stderr, "Syntax: rmlint [Target] [Options]\n");
	fprintf(stderr, "Options:\n"
					"\t-t --threads\t\tSet the number of threads used in full checksum creation.\n"
					"\t-p --printhashes\tPrint hashes of each inspected file. (true | false)\n"
					"\t-d --maxdepth\t\tOnly recurse up to this depth. (default: inf)\n"
					"\t-f --followlinks\tFollow symblic links? (true | false) (default: true)\n"
					);
	fprintf(stderr,"\t-m --mode\t\tTell autovac how to deal with the files it finds.\n"
					"\n\t\t\t\tWhere modes are:\n\n"
					"\t\t\t\tlist  - Only list found files and exit.\n"
					"\t\t\t\tlink  - Replace file with a hard link to original.\n"
					"\t\t\t\task   - Ask before removal.\n"
					"\t\t\t\tnoask - Full removal without asking.\n"
					"\t\t\t\tmove  - Move suspected files into new directory called 'autovac_out'.\n\n"
					"\t-h --help\t\tPrints this text and exits\n"
					"\t-v --version\t\tPrints version and copyright info and exits\n"
					"\n"					
					); 
					
	fprintf(stderr, "Version 0.42beta - Copyright Christopher <Bommel> Pahl\n"); 
	fprintf(stderr, "Licensed under the terms of the GPLv3 - See COPYRIGHT for more information\n");				
	fprintf(stderr, "See the manpage or the README for more information.\n");
	die(-7);
}

void rmlint_set_default_settings(rmlint_settings *set)
{
	set->mode  		 =  1; 		/* list only    */
	set->casematch   =  0; 		/* Insensitive  */
	set->invmatch	 =  0;		/* Normal mode  */
	set->paranoid	 =  0; 		/* dont be bush */
	set->depth 		 =  0; 		/* inf depth    */
	set->followlinks =  0; 		/* fol. link    */
	set->threads 	 =  4; 		/* Quad,quad.   */
	set->verbosity 	 =  3; 		/* Everything   */
	set->samepart  	 =  0; 		/* Stay parted  */
	set->paths  	 =  NULL;   /* Startnode    */
	set->dpattern 	 =  NULL;   /* DRegpattern  */
	set->fpattern 	 =  NULL;   /* FRegPattern  */
	set->cmd 		 =  NULL;   /* Cmd,if used  */
}

char rmlint_parse_arguments(int argc, char **argv, rmlint_settings *sets) 
{
	   int c,lp=0;
     
       while (1)
       {
             static struct option long_options[] =
             {
               {"threads",     required_argument, 0, 't'},
               {"dir-regex",   required_argument, 0, 'R'},
               {"file-regex",  required_argument, 0, 'r'},
               {"mode",        required_argument, 0, 'm'},
               {"maxdepth",	   required_argument, 0, 'd'},
               {"command",     required_argument, 0, 'c'},
               {"verbosity",   required_argument, 0, 'v'},
               {"matchcase",   no_argument, 	  0, 'e'},
               {"ignorecase",  no_argument, 	  0, 'E'},
               {"followlinks", no_argument, 	  0, 'f'},
               {"ignorelinks", no_argument, 	  0, 'F'},
               {"invertmatch", no_argument, 	  0, 'i'}, 
               {"normalmatch", no_argument, 	  0, 'I'},    
               {"samepart",    no_argument,		  0, 's'},
               {"allpart",     no_argument,		  0, 'S'},
               {"paranoid",    no_argument,		  0, 'p'},
               {"naive",       no_argument,		  0, 'P'},
               {"help",        no_argument, 	  0, 'h'},
               {0, 0, 0, 0}
             };
             
           /* getopt_long stores the option index here. */
           int option_index = 0;
     
           c = getopt_long (argc, argv, "m:R:r:hpPfFeEsSiIc:t:d:v:",long_options, &option_index);
     
           /* Detect the end of the options. */
           if (c == -1) break;
     
           switch (c)
           {
			     case '?': return 0; 
				 case 't':
				  
					   sets->threads = atoi(optarg);
					   if(!sets->threads || sets->threads < 0)  sets->threads = 4;
				   
				 break;
				 case 'f': sets->followlinks = 1;      		break;
				 case 'F': sets->followlinks = 0;      		break;
				 case 'h': print_help(); 			   		break;
				 case 'c': sets->cmd = optarg;  			break;
				 case 'v': sets->verbosity = atoi(optarg);  break;
				 case 'i': sets->invmatch = 1;			    break;
				 case 'I': sets->invmatch = 0;			    break;
				 case 's': sets->samepart = 1;				break;
				 case 'S': sets->samepart = 0;				break;
				 case 'e': sets->casematch = 1;				break;
				 case 'E': sets->casematch = 0;				break;
				 case 'd': sets->depth = ABS(atoi(optarg));	break;
				 case 'r': sets->fpattern = optarg;			break;
				 case 'R': sets->dpattern = optarg;			break;
				 case 'p': sets->paranoid = 1;				break;
				 case 'P': sets->paranoid = 0;				break;
				 case 'm':
				 
					if(!strcasecmp(optarg, "list"))
						sets->mode = 1; 
					if(!strcasecmp(optarg, "ask"))
						sets->mode = 2; 
					if(!strcasecmp(optarg, "noask"))
						sets->mode = 3; 
					if(!strcasecmp(optarg, "link"))
						sets->mode = 4; 
					if(!strcasecmp(optarg, "cmd"))	
						sets->mode = 5; 
					
					if(!sets->mode)
					{
						error("Invalid value for --mode: \"%s\"\n", argv[argc + 1]); 
						error("Available modes are: ask | list | link |noask | move\n"); 
						return 0; 
					}
					mode_string = optarg; 
				 
				 break;
				 default: return 0;
			 }
         }
     
         while(optind < argc)
         {
			DIR *p = opendir(argv[optind]);
			if(!p) 
			{
				error(YEL" => "RED"Can't open directory "NCO"\"%s\""RED":"NCO" %s\n"NCO, argv[optind], strerror(errno));
				return 0; 
			}
			else
			{
				closedir(p);
				sets->paths = realloc(sets->paths, sizeof(char*)*lp+2);
				sets->paths[lp++] = argv[optind];  
				sets->paths[lp  ] = NULL; 
				info(RED" => "NCO"Investigating \"%s\".\n",argv[optind]);
			}		
			optind++; 			
         }
         
         if(lp > 1) 
         {
			 char sel = 'N'; 
			 error(YEL" => "NCO"You chose to specify more than one path,\n");
			 error(YEL" => "NCO"This is possible but NOT recomned, because\n");
			 error(YEL" => "NCO"one directory may be a subset of another,\n");
			 error(YEL" => "NCO"what may lead to FALSE POSITIVES! You have been warned!\n"); 
			 error(YEL" => "NCO"Continue though? [y|N]\n");
			 error(BLU" =$ "NCO);
			 do {scanf("%c",&sel);} while ( getchar() != '\n' );
			 if(sel != 'y') return 0; 
		 }
         
         
         if(lp == 0) 
         {
			/* Still no path set? */
			sets->paths = malloc(sizeof(char*));
			sets->paths[0] = getcwd(NULL,0);
			if(!sets->paths[0]) 
			{
				error(RED"Cannot get working directory: %s\n"NCO, strerror(errno));
				if(sets->paths) free(sets->paths);
				return 0; 
			}
			use_cwd = true; 
			info(RED" => "NCO"Investigating \"%s\"\n",sets->paths);
	  }  
	  return 1; 
}


static void check_cmd(const char *cmd)
{
	int i = 0, ps = 0; 
	int len = strlen(cmd);  
	for(; i < len; i++)
	{
		 if(cmd[i] == '%' && i+1 != len)
		 {
			 if(cmd[i+1] == 's')
			 {
				ps++;
				continue;
			 } 
			 if(cmd[i+1] != '%') 
			 {
				 error("--command: Only \"%%s\" is allowed!\n"); 
				 die(-3);
			 }  
		 }
	}
	if(ps > 2) 
	{
		error("--command: Format string may only contain two \"%%s\" at the same time!\n");
		die(-2); 
	}
}

int  get_cpindex(void)
{
	return cpindex; 
}

void die(int status)
{
	/* Free mem */
	freepool(); 
	list_clear();
	
	if(use_cwd) 
	{
		if(set.paths[0])
			free(set.paths[0]);
	}
	if(set.paths)
	{
		free(set.paths);
	}
	
	
	/* Make sure NCO is printed */
	resetcol(); 
	
	/* Prepare to jump to return */
	do_exit = true;
	longjmp(place,status);
}


/* Have fun reading. ;-) */
int rmlint_main(void)
{
  UINT4 total_files = 0;
  UINT4 use_files   = 0; 
  
  int retval = setjmp(place);
  if(do_exit != true)
  {
	  if(set.cmd == NULL && set.mode == 5)
	  {
		error(YEL" => "NCO"\""YEL"-m cmd"NCO"\" needs a command specified by \""YEL"-c <CMD>"NCO"\"!\n");
		error(YEL" => Example:"NCO" rmlint . -m cmd -c \"ls -la %%s --color=auto\"\n"); 
		die(-1); 
	  }
	  
	  if(set.mode == 5)
		check_cmd(set.cmd);
	  
	  /* Give the user a status line - you will see this two calls more often */
	  info(YEL"Listing directory... \r"NCO);  
	  fflush(stdout);
	 
	  /* Count files and do some init  */ 	 
	  while(set.paths[cpindex] != NULL)
		total_files += countfiles(set.paths[cpindex++]);
	  
	  if(total_files == 0)
	  {
		  warning(RED" => "NCO"No files to search through"RED" --> "NCO"No duplicates\n"); 
		  die(0);
	  }
	  info(RED" => "NCO"Using %d thread%c.\n", set.threads, (set.threads != 1) ? 's' : ' '); 
	  info(RED" => "NCO"In total %ld usable files.\n", total_files); 
	  info(RED" => "NCO"Using mode: \"%s\".\n", (mode_string) ? mode_string : "list"); 
	 
	  /* Set threads to be less than the number of files */
	  if(set.threads > total_files) 
		set.threads = total_files;
	   
	  /* Go through directories and filter files with a unique size out */
	  use_files = filterlist();
	  
	  /* Push filtered files to md5-ToDo list */
	  pushchanges();
	  
	  /* Finally find double checksums */
	  findcrap();
	  die(0);
  }
  return retval; 
}
