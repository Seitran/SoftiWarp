/*
 * cli.c -- Command Line Interface to an OSD
 * derived from readline FileMan sample code
 *
 * Copyright (C) 2014 University of Connecticut. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <inttypes.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "osd-util/osd-util.h"
#include "osd-util/osd-defs.h"
#include "command.h"
#include "sync.h"
#include "drivelist.h"

extern char *getwd ();
extern char *xmalloc ();

/* The names of functions that actually do the manipulation. */
int com_cat(char *), com_create(char *), com_get(char *), com_help(char *);
int com_ls(char *), com_part(char *), com_put(char *), com_quit(char *);
int com_rm(char *), com_stat(char *);

/* A structure which contains information on the commands this program
   can understand. */

typedef struct {
  const char *name;		/* User printable name of the function. */
  Function *func;		/* Function to call to do the job. */
  const char *usage;		/* Usage for this function.  */
  const char *doc;		/* Documentation for this function.  */
} COMMAND;

COMMAND commands[] = {
  { "cat", com_cat, "oid", "View the contents of object oid" },
  { "create", com_create, "pid", "Create partition" },
  { "get", com_get, "oid filename [size]", "Copy OSD object to local file\n\tDefault size is 1M" },
  { "help", com_help, "", "Documentation for CLI" },
  { "ls", com_ls, "", "List objects in current partition" },
  { "part", com_part, "pid", "Change current partition" },
  { "put", com_put, "filename [oid]", "Copy local file to OSD" },
  { "quit", com_quit, "", "Quit using CLI" },
  { "rm", com_rm, "oid", "Delete object oid" },
  { "stat", com_stat, "oid", "Print out statistics on object oid" },
  { (char *)NULL, (Function *)NULL, (char *)NULL, (char *)NULL }
};

/* Forward declarations. */
char *stripwhite (char *);
COMMAND *find_command (char *);
int execute_line(char *line);
void initialize_readline (void);

/* The name of this program, as taken from argv[0]. */
char *progname;

/* When non-zero, this global means the user is done using this program. */
int done;

static char *
dupstr (s)
     char *s;
{
  char *r;

  r = xmalloc (strlen (s) + 1);
  strcpy (r, s);
  return (r);
}

static int connect_to_osd(void)
{
	int num_drives;
	struct osd_drive_description *drives;
	int ret = osd_get_drive_list(&drives, &num_drives);
	if (ret < 0) {
		printf("Could not get OSD drive list");
		return 0;
	}
	if (num_drives == 0) {
		printf("no OSD drives available\n");
		return 0;
	}
	
	int i = 0;
	printf("drive %s name %s\n", drives[i].chardev, drives[i].targetname);
	int fd = open(drives[i].chardev, O_RDWR);
	if (fd < 0) {
		printf("Could not open %s", drives[i].chardev);
		return 0;
	}
	osd_free_drive_list(drives, num_drives);

	inquiry(fd);

	return fd;
}

int osdfd;

int main (argc, argv)
     int argc;
     char **argv;
{
  char *line, *s;

  progname = argv[0];
  argc--;

  if ((osdfd = connect_to_osd()) == 0)
    return -1;

  initialize_readline ();	/* Bind our completer. */

  /* Loop reading and executing lines until the user quits. */
  for ( ; done == 0; )
    {
      line = readline ("CLI> ");

      if (!line)
        break;

      /* Remove leading and trailing whitespace from the line.
         Then, if there is anything left, add it to the history list
         and execute it. */
      s = stripwhite (line);

      if (*s)
        {
          add_history (s);
          execute_line (s);
        }

      free (line);
    }
  return 0;
}

/* Execute a command line. */
int
execute_line (char *line)
{
  register int i;
  COMMAND *command;
  char *word;

  /* Isolate the command word. */
  i = 0;
  while (line[i] && whitespace (line[i]))
    i++;
  word = line + i;

  while (line[i] && !whitespace (line[i]))
    i++;

  if (line[i])
    line[i++] = '\0';

  command = find_command (word);

  if (!command)
    {
      fprintf (stderr, "%s: No such command for OSD CLI.\n", word);
      return (-1);
    }

  /* Get argument to command, if any. */
  while (whitespace (line[i]))
    i++;

  word = line + i;

  /* Call the function. */
  return ((*(command->func)) (word));
}

/* Look up NAME as the name of a command, and return a pointer to that
   command.  Return a NULL pointer if NAME isn't a command name. */
COMMAND *
find_command (name)
     char *name;
{
  register int i;

  for (i = 0; commands[i].name; i++)
    if (strcmp (name, commands[i].name) == 0)
      return (&commands[i]);

  return ((COMMAND *)NULL);
}

/* Strip whitespace from the start and end of STRING.  Return a pointer
   into STRING. */
char *
stripwhite (string)
     char *string;
{
  register char *s, *t;

  for (s = string; whitespace (*s); s++)
    ;
    
  if (*s == 0)
    return (s);

  t = s + strlen (s) - 1;
  while (t > s && whitespace (*t))
    t--;
  *++t = '\0';

  return s;
}

/* **************************************************************** */
/*                                                                  */
/*                  Interface to Readline Completion                */
/*                                                                  */
/* **************************************************************** */

char *command_generator (char *, int);
char **cli_completion (char *, int, int);

/* Tell the GNU Readline library how to complete.  We want to try to complete
   on command names if this is the first word in the line, or on filenames
   if not. */
void initialize_readline ()
{
  /* Allow conditional parsing of the ~/.inputrc file. */
  rl_readline_name = "CLI";

  /* Tell the completer that we want a crack first. */
  rl_attempted_completion_function = (CPPFunction *)cli_completion;
}

/* Attempt to complete on the contents of TEXT.  START and END bound the
   region of rl_line_buffer that contains the word to complete.  TEXT is
   the word to complete.  We can use the entire contents of rl_line_buffer
   in case we want to do some simple parsing.  Return the array of matches,
   or NULL if there aren't any. */
char **
cli_completion (text, start, end)
     char *text;
     int start, end;
{
  char **matches;

  matches = (char **)NULL;

  /* If this word is at the start of the line, then it is a command
     to complete.  Otherwise it is the name of a file in the current
     directory. */
  if (start == 0)
    matches = completion_matches (text, command_generator);

  return (matches);
}

/* Generator function for command completion.  STATE lets us know whether
   to start from scratch; without any state (i.e. STATE == 0), then we
   start at the top of the list. */
char *
command_generator (text, state)
     char *text;
     int state;
{
  static int list_index, len;
  const char *name;

  /* If this is a new word to complete, initialize now.  This includes
     saving the length of TEXT for efficiency, and initializing the index
     variable to 0. */
  if (!state)
    {
      list_index = 0;
      len = strlen (text);
    }

  /* Return the next name which partially matches from the command list. */
  while ((name = commands[list_index].name))
    {
      list_index++;

      if (strncmp (name, text, len) == 0)
        return (dupstr(name));
    }

  /* If no names matched, then return NULL. */
  return ((char *)NULL);
}

/* Return non-zero if ARG is a valid argument for CALLER, else print
   an error message and return zero. */
static int
valid_argument (caller, arg)
     char *caller, *arg;
{
  if (!arg || !*arg)
    {
      fprintf (stderr, "%s: Argument required.\n", caller);
      return (0);
    }

  return (1);
}

/* Function which tells you that you can't do this. */
static void unimplemented (const char *cmd)
{
  fprintf (stderr, "%s command is unimplemented.\n", cmd);
}

/* **************************************************************** */
/*                                                                  */
/*                       OSD CLI Commands                           */
/*                                                                  */
/* **************************************************************** */

static uint64_t current_pid = PARTITION_PID_INVALID;

/* List the object(s) named in arg. */
int com_ls (arg)
     char *arg;
{
  unimplemented ("ls");
  return 1;
}

int com_cat (arg)
     char *arg;
{
  unimplemented ("cat");
  return 1;
}

int com_get (arg)
     char *arg;
{
  char *arg1, *arg2, *arg3;
  for(;*arg==' ';arg++);
  arg1 = arg;
  for(;*arg!=' ';arg++);
  arg2 = arg;
  for(;*arg2==' ';arg2++);
  *arg = 0;
  arg = arg2;
  for(;*arg && *arg!=' ';arg++);
  arg3 = arg;
  for(;*arg3 && *arg3==' ';arg3++);
  *arg = 0;

  uint64_t oid;
  sscanf(arg1, "%" PRIx64, &oid);

  uint64_t bufsize = 1024*1024;
  if (*arg3)
    sscanf(arg3, "%" PRId64, &bufsize);
  unsigned char *buf = malloc(bufsize);
  if (current_pid == PARTITION_PID_INVALID) {
    printf("Current partition is not set.  Use part command\n");
    return 0;
  }
  int ret = read_osd(osdfd, current_pid, oid, buf, bufsize, 0);

  int fd = open(arg2, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  write(fd, buf, bufsize);
  free(buf);

  if (ret == 0)
    printf(" Copied object 0x%llx to %s\n", llu(oid), arg2 );

  return 1;
}

int com_put (arg)
     char *arg;
{
  struct stat stat;
  char *arg1, *arg2;
  for(;*arg==' ';arg++);
  arg1 = arg;
  for(;*arg && *arg!=' ';arg++);
  arg2 = arg;
  for(;*arg2 && *arg2==' ';arg2++);
  *arg = 0;

  if (current_pid == PARTITION_PID_INVALID) {
    printf("current pid is not set\n");
    return 0;
  }

  int fd = open(arg1, O_RDONLY);
  fstat(fd, &stat);

  unsigned char *buf = malloc(stat.st_size);
  read(fd, buf, stat.st_size);

  uint64_t requested_oid=0, assigned_oid;
  if (*arg2)
    sscanf(arg2, "%" PRIx64, &requested_oid);

  int ret = create_and_write_osd(osdfd, current_pid, requested_oid, buf, 
				 stat.st_size, 0, &assigned_oid);
  if (ret == 0)
    printf(" Copied %s to object 0x%llx\n", arg1, llu(assigned_oid));

  free(buf);

  return 1;
}

int com_part (arg)
     char *arg;
{
  uint64_t pid;
  sscanf(arg, "%" PRIx64, &pid);
  current_pid = pid;
  return (1);
}

int com_stat (arg)
     char *arg;
{
  if (!valid_argument ("stat", arg))
    return (1);
  return 0;
}

int com_rm (arg)
     char *arg;
{
  unimplemented ("rm");
  return (1);
}

int com_create (arg)
     char *arg;
{
  uint64_t pid;
  sscanf(arg, "%" PRIx64, &pid);
  create_partition(osdfd, pid);
  current_pid = pid;
  return (1);
}

/* Print out help for ARG, or for all of the commands if ARG is
   not present. */
int com_help (arg)
     char *arg;
{
  register int i;
  int printed = 0;

  /* set tab at position 24 */
  printf("\r\t\033[0g\t\033[0g\r");
  for (i = 0; commands[i].name; i++)
    {
      if (!*arg || (strcmp (arg, commands[i].name) == 0))
        {
          printf ("\033[1m%s\033[0m %s\t%s.\n", commands[i].name, commands[i].usage, commands[i].doc);
          printed++;
        }
    }
  /* reset tabs */
  printf("\r\033[8C\033H\033[8C\033H\r");

  if (!printed)
    {
      printf ("No commands match `%s'.  Possibilties are:\n", arg);

      for (i = 0; commands[i].name; i++)
        {
          /* Print in six columns. */
          if (printed == 6)
            {
              printed = 0;
              printf ("\n");
            }

          printf ("%s\t", commands[i].name);
          printed++;
        }

      if (printed)
        printf ("\n");
    }
  return (0);
}

/* The user wishes to quit using this program.  Just set DONE non-zero. */
int com_quit (arg)
     char *arg;
{
  done = 1;
  return (0);
}
