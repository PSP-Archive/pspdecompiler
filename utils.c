#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "utils.h"

void report (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stdout, fmt, ap);
  va_end (ap);
}

void error (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  fprintf (stderr, "error: ");
  vfprintf (stderr, fmt, ap);
  fprintf (stderr, "\n");
  va_end (ap);
}

void xerror (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  fprintf (stderr, "error: ");
  vfprintf (stderr, fmt, ap);
  fprintf (stderr, ": %s\n", strerror (errno));
  va_end (ap);
}


void fatal (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  fprintf (stderr, "fatal: ");
  vfprintf (stderr, fmt, ap);
  fprintf (stderr, "\n");
  va_end (ap);
  exit (1);
}

void xfatal (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  fprintf (stderr, "fatal: ");
  vfprintf (stderr, fmt, ap);
  fprintf (stderr, ": %s\n", strerror (errno));
  va_end (ap);
  exit (1);
}



void *xmalloc (size_t size)
{
  void *ptr = malloc (size);
  if (!ptr) fatal ("memory exhausted");
  return ptr;
}

void *read_file (const char *path, size_t *size)
{
  FILE *fp;
  void *buffer;
  size_t file_size;
  size_t read_return;
  long r;

  fp = fopen (path, "rb");
  if (!fp) {
    xerror ("can't open file `%s'", path);
    return NULL;
  }

  if (fseek (fp, 0L, SEEK_END)) {
    xerror ("can't seek file `%s'", path);
    fclose (fp);
    return NULL;
  }

  r = ftell (fp);
  if (r == -1) {
    xerror ("can't get file size of `%s'", path);
    fclose (fp);
    return NULL;
  }

  file_size = (size_t) r;
  buffer = xmalloc (file_size);
  rewind (fp);

  read_return = fread (buffer, 1, file_size, fp);
  fclose (fp);

  if (read_return != file_size) {
    error ("can't fully read file `%s'", path);
    free (buffer);
    return NULL;
  }

  if (size) *size = file_size;
  return buffer;
}
