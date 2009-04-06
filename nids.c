
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <expat.h>

#include "nids.h"
#include "hash.h"
#include "utils.h"

struct nidstable {
  struct hashtable *libs;
  char *buffer;
};

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

  size_t buffer_pos;
  struct nidstable *result;
  struct hashtable *curlib;
  const char *libname;

  unsigned int nid;
  const char *name;
  int error;
};

static
void nids_free_callback (void *key, void *value, unsigned int hash, void *arg)
{
  struct hashtable *lib = (struct hashtable *) value;
  hashtable_free (lib, NULL, NULL);
}

void nids_free (struct nidstable *nids)
{
  if (nids->libs) hashtable_free (nids->libs, &nids_free_callback, NULL);
  nids->libs = NULL;
  if (nids->buffer) free (nids->buffer);
  nids->buffer = NULL;
  free (nids);
}


static
void start_hndl (void *data, const char *el, const char **attr)
{
  struct xml_data *d = (struct xml_data *) data;

  d->last = XMLE_DEFAULT;

  if (strcmp (el, "LIBRARY") == 0) {
    d->scope = XMLS_LIBRARY;
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
void end_hndl (void *data, const char *el)
{
  struct xml_data *d = (struct xml_data *) data;

  d->last = XMLE_DEFAULT;

  if (strcmp (el, "LIBRARY") == 0) {
    d->curlib = NULL;
    d->libname = NULL;
  } else if (strcmp (el, "FUNCTION") == 0 || strcmp (el, "VARIABLE") == 0) {
    d->scope = XMLS_LIBRARY;
    if (d->name && d->nid && d->curlib) {
      const char *name = hashtable_searchh (d->curlib, NULL, NULL, d->nid);
      if (name) {
        if (strcmp (name, d->name)) {
          error (__FILE__ ": NID `0x%08X' repeated in library `%s'", d->nid, d->libname);
          d->error = 1;
        }
      } else {
        hashtable_inserth (d->curlib, NULL, (void *) d->name, d->nid);
      }
    } else {
      error (__FILE__ ": missing function or variable definition");
      d->error = 1;
    }
    d->name = NULL;
    d->nid = 0;
  }
}

static
const char *dup_string (struct xml_data *d, const char *txt, size_t len)
{
  char *result;

  result = &d->result->buffer[d->buffer_pos];
  memcpy (result, txt, len);
  result[len] = '\0';
  d->buffer_pos += len + 1;

  return (const char *) result;
}

static
void char_hndl (void *data, const char *txt, int txtlen)
{
  struct xml_data *d = (struct xml_data *) data;

  switch (d->scope) {
  case XMLS_FUNCTION:
  case XMLS_VARIABLE:
    if (d->last == XMLE_NAME) {
      if (d->name) {
        error (__FILE__ ": repeated name in function/variable");
        d->error = 1;
      } else {
        d->name = dup_string (d, txt, txtlen);
      }
    } else if (d->last == XMLE_NID) {
      char buffer[256];

      if (txtlen > sizeof (buffer) - 1)
        txtlen = sizeof (buffer) - 1;
      memcpy (buffer, txt, txtlen);
      buffer[txtlen] = '\0';

      if (d->nid) {
        error (__FILE__ ": nid repeated in function/variable");
        d->error = 1;
      } else {
        d->nid = 0;
        sscanf (buffer, "0x%X", &d->nid);
      }
    }
    break;
  case XMLS_LIBRARY:
    if (d->last == XMLE_NAME) {
      d->libname = dup_string (d, txt, txtlen);
      if (d->curlib) {
        error (__FILE__ ": current lib is not null");
        d->error = 1;
      } else {
        d->curlib = hashtable_search (d->result->libs, (void *) d->libname, NULL);
        if (!d->curlib) {
          d->curlib = hashtable_create (128, NULL, &hashtable_pointer_compare);
          hashtable_insert (d->result->libs, (void *) d->libname, d->curlib);
        }
      }
    }
    break;
  }
}

struct nidstable *nids_load (const char *xmlpath)
{
  XML_Parser p;
  struct xml_data data;
  size_t size;
  void *buf;

  buf = read_file (xmlpath, &size);
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
  data.nid = 0;
  data.scope = XMLS_LIBRARY;
  data.last = XMLE_DEFAULT;

  data.result = (struct nidstable *) xmalloc (sizeof (struct nidstable));
  data.result->libs = hashtable_create (32, &hashtable_hash_string, &hashtable_string_compare);

  data.buffer_pos = 0;
  data.result->buffer = buf;
  buf = xmalloc (size);

  memcpy (buf, data.result->buffer, size);

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
    nids_free (data.result);
    return NULL;
  }

  return data.result;
}

static
void print_level1 (void *key, void *value, unsigned int hash, void *arg)
{
  report ("    NID: 0x%08X - Name: %s\n", hash, (char *) value);
}

static
void print_level0 (void *key, void *value, unsigned int hash, void *arg)
{
  struct hashtable *lib = (struct hashtable *) value;
  report ("  %s:\n", (char *) key);
  hashtable_traverse (lib, &print_level1, NULL);
  report ("\n");
}

void nids_print (struct nidstable *nids)
{
  report ("Libraries:\n");
  hashtable_traverse (nids->libs, &print_level0, NULL);
}


const char *nids_find (struct nidstable *nids, const char *library, unsigned int nid)
{
  struct hashtable *lib;

  lib = hashtable_search (nids->libs, (void *) library, NULL);
  if (lib) return hashtable_searchh (lib, NULL, NULL, nid);
  return NULL;
}


#ifdef TEST_NIDS
int main (int argc, char **argv)
{
  struct nidstable *nids = NULL;

  nids = nids_load (argv[1]);
  if (nids) {
    nids_print (nids);
    nids_free (nids);
  }
  return 0;
}

#endif /* TEST_NIDS */
