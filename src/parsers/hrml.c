/* 
    The contents of this file are subject to the Mozilla Public License
    Version 1.1 (the "License"); you may not use this file except in compliance
    with the License. You may obtain a copy of the License at
    http://www.mozilla.org/MPL/

    Software distributed under the License is distributed on an "AS IS" basis,
    WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
    for the specific language governing rights and limitations under the
    License.

    The Original Code is the Open Orbit space flight simulator.

    The Initial Developer of the Original Code is Mattias Holm. Portions
    created by the Initial Developer are Copyright (C) 2009 the
    Initial Developer. All Rights Reserved.

    Contributor(s):
        Mattias Holm <mattias.holm(at)openorbit.org>.

    Alternatively, the contents of this file may be used under the terms of
    either the GNU General Public License Version 2 or later (the "GPL"), or
    the GNU Lesser General Public License Version 2.1 or later (the "LGPL"), in
    which case the provisions of GPL or the LGPL License are applicable instead
    of those above. If you wish to allow use of your version of this file only
    under the terms of the GPL or the LGPL and not to allow others to use your
    version of this file under the MPL, indicate your decision by deleting the
    provisions above and replace  them with the notice and other provisions
    required by the GPL or the LGPL.  If you do not delete the provisions
    above, a recipient may use your version of this file under either the MPL,
    the GPL or the LGPL."
 */

#include "hrml.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

typedef enum HRMLtokenkind{
  HrmlTokenInvalid = 0,
  HrmlTokenSym,
  HrmlTokenStr,
  HrmlTokenInt,
  HrmlTokenFloat,
  HrmlTokenDate,
  HrmlTokenTime,
  HrmlTokenChar,
} HRMLtokenkind;

typedef struct HRMLtoken {
  HRMLtokenkind kind;
  union {
    char *sym;
    char *str;
    uint64_t integer;
    double real;
    char ch;
  } val;
} HRMLtoken;

struct growable_string {
  size_t curr_len;
  size_t a_len;
  char *str;
};

static void gw_init(struct growable_string *s)
{
  s->str = malloc(16);
  s->str[0] = '\0';
  s->a_len = 16;
  s->curr_len = 0;
}

static void gw_push(struct growable_string *s, char c)
{
  if (s->curr_len + 1 >= s->a_len) { // Add one accounts for '\0'
    s->str = realloc(s->str, s->a_len * 2);
    s->a_len *= 2;
  }
  
  s->str[s->curr_len] = c;
  s->str[s->curr_len+1] = '\0';
}

static void gw_clear(struct growable_string *s)
{
  s->curr_len = 0;
  s->str[0] = '\0';
}

static void gw_destroy(struct growable_string *s)
{
  free(s->str);
}

static size_t gw_len(struct growable_string *s) {return s->curr_len;}

static int
isbindigit(int c)
{
  if (c == '0') return 1;
  if (c == '1') return 1;
  return 0;
}

static int
isop(int c)
{
  if (c == '[') return 1;
  if (c == ']') return 1;
  if (c == '{') return 1;
  if (c == '}') return 1;

  return 0;
}

HRMLtoken
hrmlLex(FILE *f)
{
  HRMLtoken errTok = {HrmlTokenInvalid, .val.integer = 0};
  int c;
  struct growable_string s;
  gw_init(&s);

  while (isblank(c = fgetc(f))) ; // Skip WS
  if (feof(f)) return errTok; // End of file
  if (ferror(f)) {fprintf(stderr, "lexerror\n");return errTok;} // End of file

  // Skip comments
  if (c == '#') {
    while ((c = fgetc(f)) != '\n') {
      if (feof(f)) return errTok; // End of file
      if (ferror(f)) {fprintf(stderr, "lexerror\n");return errTok;} // End of file    
    }
  }

  if (c == '\n') {
    // Keep line counter up to date
  }

  // Special hex or bin numbers
  if (c == '0') {
    int c2 = fgetc(f);
    if (c2 == 'x') {
      // Hexnumber
      while (isxdigit(c = fgetc(f)) || c == '_') {
        if (c != '_') gw_push(&s, c);
      }
      if (gw_len(&s)) {
        HRMLtoken tok;
        tok.kind = HrmlTokenInt;
        tok.val.integer = strtoull(s.str, NULL, 16);
        gw_destroy(&s);
        return tok;        
      }
      return errTok;
    } else if (c2 == 'b') {
      // Binary number
      while (isbindigit(c = fgetc(f)) || c == '_') {
        if (c != '_') gw_push(&s, c);
      }
      if (gw_len(&s)) {
        HRMLtoken tok;
        tok.kind = HrmlTokenInt;
        tok.val.integer = strtoull(s.str, NULL, 2);
        gw_destroy(&s);
        return tok;
      }
      return errTok;
    } else {
      ungetc(c2, f);
    }
  }
  
  if (isdigit(c)) {
    // Integers or floats
    gw_push(&s, c);
    while (isdigit(c = fgetc(f)) || c == '_') {
      if (c != '_') gw_push(&s, c);
    }
    if (c == '.') {
      // Float
      gw_push(&s, c);
      while (isdigit(c = fgetc(f)) || c == '_') {
        if (c != '_') gw_push(&s, c);
      }
      HRMLtoken tok;
      tok.kind = HrmlTokenFloat;
      tok.val.real = strtod(s.str, NULL);
      gw_destroy(&s);
      return tok;
    } else {
      // Integer
      HRMLtoken tok;
      tok.kind = HrmlTokenInt;
      tok.val.integer = strtoull(s.str, NULL, 10);
      gw_destroy(&s);

      return tok;
    }
    return errTok;
  } else if (isalpha(c)) {
    // Symbols
    gw_push(&s, c);
    while (isalnum(c = fgetc(f))) {
      gw_push(&s, c);
    }

    HRMLtoken tok;
    tok.kind = HrmlTokenSym;
    tok.val.sym = strdup(s.str);
    gw_destroy(&s);
    return tok;
  } else if (c == '"') {
    // String
    while ((c = fgetc(f)) != '"') {
      // TODO: check for errors in reading
      if (c == '\\') {
        // Escapes
        int c2 = fgetc(f);
        if (c2 == '"') gw_push(&s, c2);
        else if (c2 == 't') gw_push(&s, '\t');
        else if (c2 == 'n') gw_push(&s, '\n');
        else if (c2 == '\\') gw_push(&s, '\\');
        else {gw_destroy(&s); return errTok; }
      } else {
        // Non escaped characters
        gw_push(&s, c);
      }
    }
    HRMLtoken tok;
    tok.kind = HrmlTokenStr;
    tok.val.str = strdup(s.str);
  }

  if (isop(c)) {
    HRMLtoken tok;
    tok.kind = HrmlTokenChar;
    tok.val.ch = c;
    gw_destroy(&s);
    return tok;
  }
  // Maybe this should be static
  gw_destroy(&s);
  return errTok;
}
#define IS_CHAR(tok, c) ((tok).kind == HrmlTokenChar && (tok).val.ch == (c))
#define IS_SYM(tok) ((tok).kind == HrmlTokenSym)
#define SYM(tok) ((tok).val.sym)

#define IS_STR(tok) ((tok).kind == HrmlTokenStr)
#define STR(tok) ((tok).val.str)
#define IS_INTEGER(tok) ((tok).kind == HrmlTokenInt)
#define IS_BOUNDED_INTEGER(tok, minVal, maxVal) ((tok).kind == HrmlTokenInt && (tok).val.integer >= (minVal) && (tok).val.integer <= (maxVal))
#define INTEGER(tok) ((tok).val.integer)
#define IS_REAL(tok) ((tok).kind == HrmlTokenFloat)
#define REAL(tok) ((tok).val.real)
#define IS_LEAP_YEAR(y) (((y) % 400 == 0) || ((y) % 100 != 0 && (y) % 4 == 0))

// Checks date for validity, asserts that month is between 1 and 12 and that the
// day is in the valid range for that month, taking leap years into account
inline bool
isValidDate(int64_t year, int month, int day)
{
  if (month == 2 && IS_LEAP_YEAR(year)) {
    if (day <= 29) {
      return true;
    } else {
      return false;
    }
  } else if (month == 2) {
    if (day <= 28) {
      return true;
    } else {
      return false;
    }
  } else if (month == 1 || month == 3 || month == 5 ||
             month == 7 || month == 8 || month == 10 ||
             month == 12) {
    if (day <= 31) {
      return true;
    } else {
      return false;
    }
  } else if (month == 4 || month == 6 || month == 9 ||
             month == 11) {
    if (day <= 31) {
      return true;
    } else {
      return false;
    }
  }
  return false;
}

HRMLobject*
hrmlParseArray(FILE *f)
{
  // At this point, the left bracket '[' should already be consumed
  HRMLtoken value = hrmlLex(f);
  HRMLtoken comma = hrmlLex(f);

  HRMLtoken rightBracket = hrmlLex(f);
}

HRMLobject*
hrmlParsePrimitiveValue(FILE *f)
{
  HRMLtoken firstTok = hrmlLex(f);
  if (IS_INTEGER(firstTok)) {
    HRMLtoken minusOrSemi = hrmlLex(f);

    if (IS_CHAR(minusOrSemi, ';')) {
      // Normal integer
    } else if (IS_CHAR(minusOrSemi, '-')) {
      // This should be a date object
      HRMLtoken month = hrmlLex(f);
      if (IS_BOUNDED_INTEGER(month, 1, 12)) {
        HRMLtoken minus1 = hrmlLex(f);
        if (IS_CHAR(minus1, '-')) {
          HRMLtoken day = hrmlLex(f);
          if (IS_BOUNDED_INTEGER(day, 1, 31)) {
            HRMLtoken semiOrHour = hrmlLex(f);
            if (IS_CHAR(semiOrHour, ';')) {
              // Date ok... sort of, we still need to verify the days in the
              // month
              int64_t yearNum = INTEGER(firstTok);
              int monthNum = INTEGER(month);
              int dayNum = INTEGER(day);
              if (isValidDate(yearNum, monthNum, dayNum)) {
                
              }
            } else if (IS_BOUNDED_INTEGER(semiOrHour, 0, 23)) {

            }
          } 
        }
      }
    } else {
      // Parse error
    }
  } else if (IS_REAL(firstTok)) {
    HRMLtoken unitOrSemi = hrmlLex(f);
    if (IS_CHAR(unitOrSemi, ';')) {
      
    } else if (IS_SYM(unitOrSemi)) {
      
    }
  }
}


HRMLobject*
hrmlParseObj(FILE *f)
{
  HRMLtoken nameOrColon = hrmlLex(f);
  if (nameOrColon.kind == HrmlTokenChar && nameOrColon.val.ch == ':') {
    // Anonymous object, they are always primitive
    HRMLtoken val = hrmlLex(f);
  } else if (nameOrColon.kind == HrmlTokenSym){
    HRMLtoken colonOrBrace = hrmlLex(f);
    if (IS_CHAR(colonOrBrace, ':')) {
      
    } else if (IS_CHAR(colonOrBrace, '{')) {
      // New object
    } else {
      // Parse errror
    }
  } else {
      // Parse errror    
  }
}

static HRMLlist*
hrmlParse2(FILE *f, HRMLlist *node)
{
  HRMLtoken tok = hrmlLex(f);
  HRMLobject *obj = NULL;
  switch (tok.kind) {
  case HrmlTokenInvalid:
    break;
  case HrmlTokenSym:
    break;
  case HrmlTokenStr:
    break;
  case HrmlTokenInt:
    break;
  case HrmlTokenFloat:
    break;
  case HrmlTokenDate:
    break;
  case HrmlTokenTime:
    break;
  case HrmlTokenChar:
    switch (tok.val.ch) {
    case '[':
      obj = hrmlParseArray(f);
      break;
    case '{':
      obj = hrmlParseObj(f);
      break;
    }
    break;
  default:
    assert(0 && "invalid case");
  }
}

HRMLdocument*
hrmlParse(FILE *f)
{
  HRMLdocument *doc = malloc(sizeof(HRMLdocument));
  doc->rootNode = malloc(sizeof(HRMLlist));
  
  return NULL;
}

bool
hrmlValidate(HRMLdocument *doc, HRMLschema *sc)
{
  
}

HRMLiterator *
hrmlRootIterator(HRMLdocument *doc)
{
  HRMLiterator *it = malloc(sizeof(HRMLiterator));
  
  return it;
}


HRMLiterator *
hrmlIteratorNext(HRMLiterator *it)
{
  if (it->next) {
    return it->next;
  }
  
  return NULL;
}

HRMLiterator *
hrmlIteratorPrev(HRMLiterator *it)
{
  if (it->previous) {
    return it->previous;
  }
  
  return NULL;
}


HRMLtype hrmlIteratorType(HRMLiterator *it)
{
  return it->data->typ;
}
HRMLobject* hrmlIteratorValue(HRMLiterator *it)
{
  return it->data;
}
