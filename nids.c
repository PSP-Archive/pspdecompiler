
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <expat.h>
#include "nids.h"
#include "utils.h"

enum XMLSCOPE {
  XMLS_LIBRARY,
  XMLS_FUNCTION,
  XMLS_VARIABLE
};

enum XMLELEMENT {
  XMLE_DEFAULT,
  XMLE_NID,
  XMLE_NAME
};

struct xml_data {
  enum XMLSCOPE scope;
  enum XMLELEMENT last;

  struct hashtable *ht;
  struct hashtable *curlib;
  const char *libname;


  const void *nid;
  const char *name;
  int error;
};

static
void destroy_nid_entry (void *key, void *value, void *arg)
{
  free (value);
}

static
void destroy_library (struct hashtable *ht)
{
  hashtable_destroy (ht, &destroy_nid_entry, NULL);
}

static
void destroy_library_entry (void *key, void *value, void *arg)
{
  struct hashtable *ht = (struct hashtable *) value;
  destroy_library (ht);
  free (key);
}

void nids_free (struct hashtable *nids)
{
  hashtable_destroy (nids, &destroy_library_entry, NULL);
}


static
void start_hndl (void *data, const char *el, const char **attr)
{
  struct xml_data *d = (struct xml_data *) data;

  d->last = XMLE_DEFAULT;

  if (strcmp (el, "LIBRARY") == 0) {
    d->scope = XMLS_LIBRARY;
    if (d->curlib) {
      error (__FILE__ ": current lib is not null");
      destroy_library (d->curlib);
      d->error = 1;
    }
    d->curlib = hashtable_create (128, &hashtable_hash_pointer, &hashtable_pointer_compare);
  } else if (strcmp (el, "NAME") == 0) {
    d->last = XMLE_NAME;
  } else if (strcmp (el, "FUNCTION") == 0) {
    d->scope = XMLS_FUNCTION;
  } else if (strcmp (el, "VARIABLE") == 0) {
    d->scope = XMLS_VARIABLE;
  } else if (strcmp (el, "NID") == 0) {
    d->last = XMLE_NID;
  }
}

static
void merge_libraries (void *outkey, void *outvalue, void *inkey, void *invalue, void *arg)
{
  struct xml_data *d = (struct xml_data *) arg;
  if (strcmp (invalue, outvalue)) {
    error (__FILE__ ": NID `%s' repeated in library `%s'", (char *) inkey, d->libname);
    d->error = 1;
  }
  free (invalue);
}

static
void end_hndl (void *data, const char *el)
{
  struct xml_data *d = (struct xml_data *) data;

  d->last = XMLE_DEFAULT;

  if (strcmp (el, "LIBRARY") == 0) {
    if (d->libname && d->curlib) {
      struct hashtable *ht = hashtable_search (d->ht, (void *) d->libname, NULL);
      if (ht) {
        hashtable_merge (ht, d->curlib, &merge_libraries, data);
        hashtable_destroy (d->curlib, NULL, NULL);
        free ((void *) d->libname);
      } else {
        hashtable_insert (d->ht, (void *) d->libname, (void *) d->curlib);
      }
    } else {
      if (d->libname) free ((void *) d->libname);
      if (d->curlib) destroy_library (d->curlib);
      error (__FILE__ ": missing library definition");
      d->error = 1;
    }
    d->curlib = NULL;
    d->libname = NULL;
  } else if (strcmp (el, "FUNCTION") == 0 || strcmp (el, "VARIABLE") == 0) {
    d->scope = XMLS_LIBRARY;
    if (d->name && d->nid) {
      if (hashtable_search (d->curlib, (void *) d->nid, NULL)) {
        error (__FILE__ ": NID `0x%08X' repeated in library `%s'", d->nid, d->libname);
        free ((void *) d->name);
        d->error = 1;
      } else {
        hashtable_insert (d->curlib, (void *) d->nid, (void *) d->name);
      }
    } else {
      if (d->name) free ((void *) d->name);
      error (__FILE__ ": missing function or variable definition");
      d->error = 1;
    }
    d->name = NULL;
    d->nid = NULL;
  }
}

static
const char *dup_string (const char *str, int size)
{
  char *c = (char *) xmalloc (size + 1);
  memcpy (c, str, size);
  c[size] = '\0';
  return (const char *) c;
}

static
void char_hndl (void *data, const char *txt, int txtlen)
{
  char buffer[16];
  struct xml_data *d = (struct xml_data *) data;

  switch (d->scope) {
  case XMLS_FUNCTION:
  case XMLS_VARIABLE:
    if (d->last == XMLE_NAME) {
      if (d->name) {
        error (__FILE__ ": name is not null");
        free ((void *) d->name);
        d->error = 1;
      }
      d->name = dup_string (txt, txtlen);
    } else if (d->last == XMLE_NID) {
      unsigned int ptr = 0;
      if (txtlen > 10) txtlen = 10;
      memcpy (buffer, txt, txtlen);
      buffer[txtlen] = '\0';
      if (d->nid) {
        error (__FILE__ ": nid is not null");
        d->error = 1;
      }
      d->nid = NULL;
      sscanf (buffer, "0x%X", &ptr);
      d->nid = (const void *) ptr;
    }
    break;
  case XMLS_LIBRARY:
    if (d->last == XMLE_NAME) {
      if (d->libname) {
        error (__FILE__ ": library name is not null");
        free ((void *) d->libname);
        d->error = 1;
      }
      d->libname = dup_string (txt, txtlen);
    }
    break;
  }
}

struct hashtable *nids_load_xml (const char *path)
{
  XML_Parser p;
  struct xml_data data;
  size_t size;
  void *buf;

  buf = read_file (path, &size);
  if (!buf) {
    return NULL;
  }

  p = XML_ParserCreate (NULL);
  if (!p) {
    error (__FILE__ ": can't create XML parser");
    free (buf);
    return 0;
  }

  data.error = 0;
  data.curlib = NULL;
  data.libname = NULL;
  data.name = NULL;
  data.nid = NULL;
  data.scope = XMLS_LIBRARY;
  data.last = XMLE_DEFAULT;

  data.ht = hashtable_create (32, &hashtable_hash_string, &hashtable_string_compare);

  XML_SetUserData (p, (void *) &data);
  XML_SetElementHandler (p, &start_hndl, &end_hndl);
  XML_SetCharacterDataHandler (p, &char_hndl);

  if (!XML_Parse (p, buf, size, 1)) {
    error (__FILE__ ": parse error at line %d:\n  %s\n", XML_GetCurrentLineNumber (p),
           XML_ErrorString (XML_GetErrorCode (p)));
    data.error = 1;
  }

  XML_ParserFree (p);
  free (buf);

  if (data.error) {
    nids_free (data.ht);
    return NULL;
  }

  return data.ht;
}

static
void print_level1 (void *key, void *value, void *arg)
{
  report ("    NID: 0x%08X - Name: %s\n", key, (char *) value);
}

static
void print_level0 (void *key, void *value, void *arg)
{
  struct hashtable *ht = (struct hashtable *) value;
  report ("  %s:\n", (char *) key);
  hashtable_traverse (ht, &print_level1, NULL);
  report ("\n");
}

void nids_print (struct hashtable *nids)
{
  report ("Libraries:\n");
  hashtable_traverse (nids, &print_level0, NULL);
}
