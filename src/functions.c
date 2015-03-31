/*
 *  TurboXSL XML+XSLT processing library
 *  XPATH functions including ltr: and dbg: namespace extensions
 *
 *
 *  (c) Egor Voznessenski, voznyak@mail.ru
 *
 *  $Id$
 *
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include "ltr_xsl.h"
#include "md5.h"
#include "external_cache.h"

char *call_perl_function(TRANSFORM_CONTEXT *pctx, void *function, char **args)
{
  if (pthread_mutex_lock(&(pctx->lock))) {
    error("call_perl_function:: lock");
    return NULL;
  }
  char *result = (pctx->gctx->perl_cb_dispatcher)(function, args, pctx->gctx->interpreter);
  if (pthread_mutex_unlock(&(pctx->lock))) {
    error("call_perl_function:: unlock");
  }
  return result;
}

void xf_strescape(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  char *str;
  char *p;
  int js = 1;

  xpath_execute_scalar(pctx, locals, args, current, &rv);
  str = rval2string(&rv);
  if(args->next) {
    char *mode;
    xpath_execute_scalar(pctx, locals, args->next, current, &rv);
    mode = rval2string(&rv);
    if(xml_strcmp(mode,"js"))
      js = 0;
  }
  res->type = VAL_STRING;
  if(js && str) {
    XMLSTRING s = xmls_new(200);
    for(p=str;*p;++p) {
      switch(*p) {
        case '\'':
        case '\"':
        case '\\':
        case '/':
          xmls_add_char(s,'\\');
          xmls_add_char(s,*p);
          break;
        case '\r':
        case '\n':
          xmls_add_char(s,'\\');
          xmls_add_char(s,'n');
          break;
        default:
          xmls_add_char(s,*p);
      }
    }
    res->v.string = xmls_detach(s);
  } else {
    res->v.string = str;
  }
}

void xf_getid(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  char buf[100];
  RVALUE rv;

  if(args) {
    xpath_execute_scalar(pctx, locals, args, current, &rv);
    current = NULL;
    if(rv.type == VAL_NODESET) {
      current = rv.v.nodeset;
    }
  }
  if(current) {
    sprintf(buf,"id_%x",current->uid);
    trace("xf_getid:: id: %s", buf);
    res->type = VAL_STRING;
    res->v.string = xml_strdup(buf);
  } else {
    res->type = VAL_NULL;
  }
}

void xf_current(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  XMLNODE *set;
  unsigned p = 0;

  set = add_to_selection(NULL,locals->parent,&p);
  res->type = VAL_NODESET;
  res->v.nodeset = set;
}

void xf_true(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_BOOL;
  res->v.integer = 1;
}

void xf_false(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_BOOL;
  res->v.integer = 0;
}

void xf_last(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_INT;
  res->v.integer = 1;

  if(current) {
    for(;current->next; current=current->next)
	   ;
    
    res->v.integer = current->position;
  }
}

void xf_concat(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  char *s = NULL;
  char *t;
  for(;args;args=args->next) {
    xpath_execute_scalar(pctx, locals, args, current, &rv);
    if(s) {
      t = rval2string(&rv);
      if(t) {
        char *data = s;
        size_t data_size = strlen(s);
        s = memory_allocator_new(data_size + strlen(t) + 3);
        memcpy(s, data, data_size);
        strcat(s,t);
      }
    } else {
      s = rval2string(&rv);
    }
  }  
  res->type = VAL_STRING;
  res->v.string = s;
}

void xf_substr(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_STRING;

  RVALUE rv;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  char *s = rval2string(&rv);
  if(!s) {
    res->v.string = NULL;
    return;
  }


  xpath_execute_scalar(pctx, locals, args->next, current, &rv);
  int start = (int)floor(rval2number(&rv));

  int length = INT_MAX;
  if(args->next->next) {
    xpath_execute_scalar(pctx, locals, args->next->next, current, &rv);
    length = (int)floor(rval2number(&rv));
    if (length < 0) {
      res->v.string = NULL;
      return;
    }
  }

  debug("xf_substr:: start: %d, length: %d", start, length);

  short *src = utf2ws(s);

  XMLSTRING xs = xmls_new(100);
  size_t p = 0;
  while (src[p] != 0) {
    if (p + 1 >= start && p + 1 < start + length) xmls_add_utf(xs, src[p]);
    p = p + 1;
  }

  res->v.string = xmls_detach(xs);
}

void xf_tostr(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  char *s;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  s = rval2string(&rv);
  res->type = VAL_STRING;
  res->v.string = s;
}

void xf_tonum(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  double d;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  d = rval2number(&rv);
  res->type = VAL_NUMBER;
  res->v.number = d;
}

void xf_tobool(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  int d;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  d = rval2bool(&rv);
  res->type = VAL_BOOL;
  res->v.integer = d;
}

void xf_round(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  double d;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  d = rval2number(&rv);
  res->type = VAL_NUMBER;
  res->v.number = floor(d+0.5);
}

void xf_floor(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  double d;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  d = rval2number(&rv);
  res->type = VAL_NUMBER;
  res->v.number = floor(d);
}

void xf_ceil(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  double d;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  d = rval2number(&rv);
  res->type = VAL_NUMBER;
  res->v.number = ceil(d);
}

void xf_sum(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  double d = 0;
  XMLNODE *node;

  while(args) {
    xpath_execute_scalar(pctx, locals, args, current, &rv);
    if(rv.type==VAL_NODESET) {
      for(node=rv.v.nodeset;node;node=node->next) {
        char *s = node2string(node);
        d = d + strtod(s, NULL);
      }
    } else {
      d = d + rval2number(&rv);
    }
    args = args->next;
  }
  res->type = VAL_NUMBER;
  res->v.number = d;
}

void xf_contains(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  char *s,*p;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  s = rval2string(&rv);
  xpath_execute_scalar(pctx, locals, args->next, current, &rv);
  p = rval2string(&rv);
  res->type = VAL_BOOL;
  res->v.integer = 0;
  if(s && p && strstr(s,p))
    res->v.integer = 1;
}

void xf_starts(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  char *s,*p;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  s = rval2string(&rv);
  xpath_execute_scalar(pctx, locals, args->next, current, &rv);
  p = rval2string(&rv);
  res->type = VAL_BOOL;
  res->v.integer = 0;
  if(s && p && strstr(s,p)==s)
    res->v.integer = 1;
}

void xf_sub_before(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  char *s,*p,*z;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  s = rval2string(&rv);
  xpath_execute_scalar(pctx, locals, args->next, current, &rv);
  p = rval2string(&rv);
  res->type = VAL_STRING;
  res->v.string = 0;
  if(s && p && p[0]) {
    z = strstr(s,p);
    if(z) {
      *z =0;
      res->v.string = s;
    }
  }
}

void xf_sub_after(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  char *s,*p,*z;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  s = rval2string(&rv);
  xpath_execute_scalar(pctx, locals, args->next, current, &rv);
  p = rval2string(&rv);
  res->type = VAL_STRING;
  res->v.string = 0;
  if(s && p && p[0]) {
    z = strstr(s,p);
    if(z) {
      z+=strlen(p);
      res->v.string = xml_strdup(z);
    }
  }
}

void xf_count(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  unsigned n = 0;
  RVALUE rv;
  XMLNODE *selection, *t;

  xpath_execute_scalar(pctx, locals, args, current, &rv);
  if(rv.type == VAL_NODESET) {
    for(selection=rv.v.nodeset;selection;selection=selection->next) {
      if(selection->type==EMPTY_NODE) {
        for(t=selection->children;t;t=t->next)
          ++n;
      } else {
        ++n;
      }
    }
  }
  rval_free(&rv);

  res->type = VAL_INT;
  res->v.integer = n;
}

void xf_normalize(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  char *s, *p;
  int i;

  rv.type = VAL_NULL;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  s = rval2string(&rv);
  res->type = VAL_STRING;
  res->v.string = s;
  if(s) {
    for(p=s;x_is_ws(*p);++p)  //skip leading space
        ;
    while(*p) { //copy content
      *s++ = *p;
      if(x_is_ws(*p)) {
        while(x_is_ws(*p))
          ++p;
      } else ++p;
    }
    *s=0;
    s = res->v.string;  // strip trailing ws
    for(i=strlen(s)-1;i>=0;--i) {
      if(!x_is_ws(s[i]))
        break;
      s[i]=0;
    }
  }
}

void xf_strlen(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  unsigned char *s, *p;
  int i;

  rv.type = VAL_NULL;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  s = rval2string(&rv);
  i = 0;
  if(s) {
    for(p=s;*p;++p) {
      if((0x0C0 & *p)==0x080)
        continue;
      ++i;
    }
  }
  res->type = VAL_INT;
  res->v.integer = i;
}


// format-number
void xf_format(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_STRING;

  char *decimal_separator_sign = ".";
  char *grouping_sign = ",";
  char *percent_sign = "%";
  char *digit_zero_sign = "0";
  char *digit_sign = "#";
  char *pattern_separator_sign = ";";
  char *infinity_symbol = "Infinity";
  char *nan_symbol = "NaN";
  char *minus_symbol = "-";

  RVALUE rv;
  rv.type = VAL_NULL;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  double number = rval2number(&rv);

  xpath_execute_scalar(pctx, locals, args->next, current, &rv);
  char *pattern = rval2string(&rv);
  size_t pattern_length = strlen(pattern);
  if (pattern_length > 120) {
    error("xf_format:: pattern length error");
    res->v.string = NULL;
    return;
  }

  if (args->next->next) {
    xpath_execute_scalar(pctx, locals, args->next->next, current, &rv);
    char *format_name = rval2string(&rv);

    XMLNODE *format = pctx->formats;
    while (format != NULL) {
      if (strcmp(format->name, format_name) == 0) {
        break;
      }
      format = format->next;
    }
    if (format == NULL) {
      error("xf_format:: unknown decimal format: %s", format_name);
      res->v.string = NULL;
      return;
    }

    debug("xf_format:: decimal format: %s", format_name);
    XMLNODE *node = format->children;
    char *value = xml_get_attr(node, "decimal-separator");
    if (value != NULL) decimal_separator_sign = value;

    value = xml_get_attr(node, "grouping-separator");
    if (value != NULL) grouping_sign = value;

    value = xml_get_attr(node, "percent");
    if (value != NULL) percent_sign = value;

    value = xml_get_attr(node, "zero-digit");
    if (value != NULL) digit_zero_sign = value;

    value = xml_get_attr(node, "digit");
    if (value != NULL) digit_sign = value;

    value = xml_get_attr(node, "pattern-separator");
    if (value != NULL) pattern_separator_sign = value;

    value = xml_get_attr(node, "infinity");
    if (value != NULL) infinity_symbol = value;

    value = xml_get_attr(node, "NaN");
    if (value != NULL) nan_symbol = value;

    value = xml_get_attr(node, "minus-sign");
    if (value != NULL) minus_symbol = value;
  }

  if (strstr(pattern, pattern_separator_sign) != NULL) {
    error("xf_format:: patten separator not supported");
    res->v.string = NULL;
    return;
  }

  if (strstr(pattern, percent_sign) != NULL) {
    error("xf_format:: percent not supported");
    res->v.string = NULL;
    return;
  }

  if (isnan(number)) {
    res->v.string = xml_strdup(nan_symbol);
    return;
  }

  if (isinf(number)) {
    res->v.string = xml_strdup(infinity_symbol);
    return;
  }

  // split pattern into integer and fractional parts
  char integer_part[64];
  memset(integer_part, 0, sizeof(integer_part));
  size_t integer_part_size = 0;

  char fractional_part[64];
  memset(fractional_part, 0, sizeof(fractional_part));
  size_t fractional_part_size = 0;

  char *p = strstr(pattern, decimal_separator_sign);
  if (p != NULL) {
    integer_part_size = p - pattern;
    if (integer_part_size != 0) {
      memcpy(integer_part, pattern, integer_part_size);
    }
    fractional_part_size = pattern_length - integer_part_size - 1;
    if (fractional_part_size > 0) memcpy(fractional_part, p + 1, fractional_part_size);
  } else {
    integer_part_size = pattern_length;
    memcpy(integer_part, pattern, integer_part_size);
  }

  char output[128];
  memset(output, 0, sizeof(output));
  unsigned int output_position = 0;

  if (signbit(number)) output[output_position++] = minus_symbol[0];

  double t = 0;
  double fractional_number = modf(fabs(number), &t);
  unsigned int integer_number = (unsigned int)floor(t);

  // integer part variables
  unsigned int minimum_integer_part_size = 0;
  unsigned int maximum_integer_part_size = 0;
  unsigned int integer_part_grouping_position = 0;
  debug("xf_format:: integer part: %s", integer_part);
  for (size_t i = integer_part_size; i != 0; i--) {
    char c = integer_part[i - 1];
    if (c == digit_zero_sign[0]) {
      minimum_integer_part_size++;
      maximum_integer_part_size++;
    }
    if (c == digit_sign[0]) maximum_integer_part_size++;

    if (c == grouping_sign[0] && integer_part_grouping_position == 0)
      integer_part_grouping_position = maximum_integer_part_size;
  }
  if (minimum_integer_part_size == 0) minimum_integer_part_size = 1;

  // process integer part
  char integer_number_string[32];
  memset(integer_number_string, 0, sizeof(integer_number_string));
  unsigned int precision = (unsigned int)floor(log10(integer_number)) + 1;
  if (precision > sizeof(integer_number_string)) {
    error("xf_format:: integer number error");
    res->v.string = NULL;
    return;
  }
  if (precision < minimum_integer_part_size) precision = minimum_integer_part_size;
  for (int i = 0; i < precision; i++) {
    int digit = integer_number % 10;
    if (i < minimum_integer_part_size) {
      if (digit != 0) {
        integer_number_string[i] = (char)(digit + 0x30);
      } else {
        integer_number_string[i] = '0';
      }
    } else {
      integer_number_string[i] = (char)(digit + 0x30);
    }
    integer_number = integer_number / 10;
  }

  for (int i = 0; i < precision; i++) {
    // grouping
    if (integer_part_grouping_position > 0 && integer_part_grouping_position < precision) {
      if ((precision - i) % integer_part_grouping_position == 0) {
        output[output_position++] = grouping_sign[0];
      }
    }
    output[output_position++] = integer_number_string[precision - i - 1];
  }

  if (fractional_part_size > 0) {
    // fractional part variables
    unsigned int minimum_fractional_part_size = 0;
    unsigned int maximum_fractional_part_size = 0;
    unsigned int fractional_part_grouping_position = 0;
    debug("xf_format:: fractional part: %s", fractional_part);
    for (size_t i = 0; i < fractional_part_size; i++) {
      char c = fractional_part[i];
      if (c == digit_zero_sign[0]) {
        minimum_fractional_part_size++;
        maximum_fractional_part_size++;
      }
      if (c == digit_sign[0]) maximum_fractional_part_size++;
      if (c == grouping_sign[0] && fractional_part_grouping_position == 0)
        fractional_part_grouping_position = i;
    }

    // process fractional part
    output[output_position++] = decimal_separator_sign[0];

    for (unsigned int i = 0; i < maximum_fractional_part_size; i++) {
      int digit = (int)floor(pow(10, i + 1) * fractional_number) % 10;
      if (digit != 0) {
        output[output_position++] = (char)(digit + 0x30);
      } else {
        if (i < minimum_fractional_part_size) output[output_position++] = '0';
      }
    }
  }

  res->type = VAL_STRING;
  res->v.string = xml_strdup(output);
}

void xf_translate(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_STRING;
  RVALUE rv;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  char *s = rval2string(&rv);
  if(!s) {
    res->v.string = NULL;
    return;
  }

  if(args->next == NULL || args->next->next == NULL) {
    res->v.string = NULL;
    return;
  }

  short *src = utf2ws(s);

  xpath_execute_scalar(pctx, locals, args->next, current, &rv);
  s = rval2string(&rv);
  short *pat = utf2ws(s);

  xpath_execute_scalar(pctx, locals, args->next->next, current, &rv);
  s = rval2string(&rv);
  short *sub = utf2ws(s);

  XMLSTRING xs = xmls_new(100);
  for(int i=0;src[i];++i) {
    short v = src[i];
    for(int j=0;pat[j];++j) {
      if(v==pat[j]) {
        v = sub[j];
        continue;
      }
    }
    if (v != 0) xmls_add_utf(xs,v);
  }
  xmls_add_char(xs,0);

  res->v.string = xmls_detach(xs);
}

void xf_name(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  char *s;
  RVALUE rv;
  rv.type = VAL_NULL;

  if(args) {
    xpath_execute_scalar(pctx, locals, args, current, &rv);
    current = NULL;
    if(rv.type == VAL_NODESET) {
      current = rv.v.nodeset;
    }
  }
  if(current && current->name)
    s = xml_strdup(current->name);
  else
    s = xml_strdup("");

  res->type = VAL_STRING;
  res->v.string = s;
  rval_free(&rv);
}

void xf_lname(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  char *s;
  if(current && current->name) {
    s = strchr(current->name,':');
    if(s) {
      s = xml_strdup(s+1);
    } else {
      s = xml_strdup(current->name);
    }
  }
  else
    s = xml_strdup("");

  res->type = VAL_STRING;
  res->v.string = s;
}

void xf_position(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_INT;
  res->v.integer = current->position;
}


void xf_document(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE   rv;
  XMLNODE *doc = NULL;
  char    *docname;
  char    *abs;
  unsigned p = 0;

  xpath_execute_scalar(pctx, locals, args, current, &rv);
  docname = rval2string(&rv);
  debug("xf_document:: document name: %s", docname);
  if (docname[0] == 0) {
    doc = add_to_selection(NULL, pctx->stylesheet, &p);
  } 
  else {
    abs = get_abs_name(pctx, docname);
    debug("xf_document:: absolute name: %s", abs);
    if(abs) {
      doc = xml_parse_file(pctx->gctx, abs, 1);
      doc = add_to_selection(NULL, doc, &p);
    }
  }

  if(doc) {
    res->type = VAL_NODESET;
    res->v.nodeset = doc;
  } 
  else {
    res->type = VAL_NULL;
  }
}


void xf_text(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  XMLNODE *texts = NULL;
  XMLNODE *ret = NULL;
  XMLNODE *iter;
  unsigned p = 0;

  for(iter=current->children;iter;iter=iter->next) {
    if(iter->type == TEXT_NODE) {
      texts = add_to_selection(texts, iter, &p);
      if(ret == NULL)
        ret = texts;
    }
  }
  res->type = VAL_NODESET;
  res->v.nodeset = ret;
}


void xf_check(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  char *str;
  char *rights;
  res->type = VAL_BOOL;
  res->v.integer = 0;

  if(args) {
    xpath_execute_scalar(pctx, locals, args, current, &rv);
    str=rval2string(&rv);
    if(str) {
      rights = xsl_get_global_key(pctx, "rights",NULL);
      if(rights) {
        if(strstr(rights,str))
          res->v.integer = 1;
      }
    }
  }
}

void xf_exists(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_INT;
  res->v.integer = 1;
}

void xf_md5(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  char *str;
  XMLSTRING msg = xmls_new(200);
  char md5sig[20];
  char md5hex[40];
  RVALUE rv;

  for(;args;args=args->next) {
    xpath_execute_scalar(pctx, locals, args, current, &rv);
    str = rval2string(&rv);
    if(str) {
      xmls_add_str(msg, str);
    }
  }
  str = xmls_detach(msg);
  md5_buffer(str,strlen(str),md5sig);
  md5_sig_to_string(md5sig,md5hex,33);
  res->type = VAL_STRING;
  res->v.string = xml_strdup(md5hex);
}

void xf_base64(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_STRING;
  res->v.string = xml_strdup("Aa3b24455632+30h");
}

static char *url_get_specials(TRANSFORM_CONTEXT *pctx, char *name)
{
  return NULL;
}

void xf_urlcode(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE     rv;
  XMLSTRING  url, key;
  char      *str;
  char      *p;
  char      *arg;
  char       separ = '?';
  char      *value;

  res->type = VAL_STRING;
  if(args==NULL) {
    debug("xf_urlcode:: no arguments, return default value");
    res->v.string = xml_strdup("/");
    return;
  }

  if(pctx->gctx->perl_urlcode) {
    XMLNODE *parg;
    char *p_args[2];
    key = xmls_new(100);
    for(parg=args;parg;parg=parg->next) {
      xpath_execute_scalar(pctx, locals, parg, current, &rv);
      str = rv.type == VAL_NODESET ? nodes2string(rv.v.nodeset->children) : rval2string(&rv);
      xmls_add_str(key,str);
      if(parg->next)
        xmls_add_char(key,',');
    }
    if(key->len == 0) {
      debug("xf_urlcode:: key is empty, return default value");
      res->v.string = xml_strdup("/");
      return;
    }

    str = xmls_detach(key);
    p = dict_find(pctx->gctx->urldict,str);
    if(!p) {
      p = external_cache_get(pctx->gctx->cache, str);
      if(!p) {
        p_args[0] = str;
        p_args[1] = NULL;
        p = call_perl_function(pctx, pctx->gctx->perl_urlcode, p_args);
        external_cache_set(pctx->gctx->cache, str, p);
        dict_add(pctx->gctx->urldict,str,p);
      }
    }
    res->v.string = xml_strdup(p);
    return;
  }
  url = xmls_new(100);
  xmls_add_char(url,'/');
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  str = rval2string(&rv);
  if(str && str[0]) {
    xmls_add_str(url,"pages/");
    if(p = strchr(str,',')) {
      *p++ = 0;
      xmls_add_str(url,str);
      while(p && *p) {
        value = NULL;
        arg = p;
        p = strchr(arg,',');
        if(p)
          *p++=0;
        if(arg[0]=='-') {
          value = url_get_specials(pctx,arg+1);
        } else if(p) {
          value = p;
          p = strchr(value,',');
          if(p)
            *p++ = 0;
        }
        if(value) {
          xmls_add_char(url,separ);
          xmls_add_str(url,arg);
          xmls_add_char(url,'=');
          xmls_add_str(url,value);
          separ = '&';
        }
      }
    } else {
      xmls_add_str(url,str);
      for(args=args->next;args;args=args->next) { // start from second arg
        xpath_execute_scalar(pctx, locals, args, current, &rv);
         str = rval2string(&rv);
         if(str) {
          char *ps = str;
          value = NULL;
          if(str[0] == '-') {
            value = url_get_specials(pctx,ps=str+1);
          } else if(args->next) {
            args = args->next;
            xpath_execute_scalar(pctx, locals, args, current, &rv);
            value = rval2string(&rv);
          }
          if(value) {
            xmls_add_char(url,separ);
            xmls_add_str(url,ps);
            xmls_add_char(url,'=');
            xmls_add_str(url,value);
            separ = '&';
          }
        }
      }
    }
  }

  if(separ=='?') // add trailer / for urls with no args XXX - not necessary! just for ltr:url_code compatability
    xmls_add_char(url,'/');

  res->v.string = xmls_detach(url);
}


void xf_veristat(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  XMLSTRING url;
  char *str;
  char separ='?';

  res->type = VAL_STRING;
  if(args==NULL) {
    res->v.string = NULL;
    return;
  }
  url = xmls_new(100);
  xmls_add_str(url,"/static/");

  xf_concat(pctx,locals,args,current,&rv);
  str = rval2string(&rv);
  if(str) {
    char *rev;
    xmls_add_str(url,str);
    rev = xsl_get_global_key(pctx, "staticFileRevisions",str);
    if(rev) {
      xmls_add_char(url,'?');
      xmls_add_str(url,rev);
    }
  }

  res->v.string = xmls_detach(url);
}

void xf_urlenc(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  XMLSTRING url;
  char *str,*tofree;
  char c;

  res->type = VAL_STRING;

  if (args == NULL) {
    res->v.string = NULL;
    return;
  }

  url = xmls_new(100);
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  tofree = str = rval2string(&rv);

  if (str != NULL) {
    for(; c = *str; ++str) {
      if(c=='?'){
        xmls_add_str(url,"%3F");
      }
      else if(c==' ') {
        xmls_add_str(url,"%20");
      }
      else if(c=='&') {
        xmls_add_str(url,"%26");
      }
      else {
        xmls_add_char(url,c);
      }
    }
  }

  res->v.string = xmls_detach(url);
}

void xf_veristatl(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  XMLSTRING url;
  char *str;
  char separ='?';
  char *rev;

  res->type = VAL_STRING;
  if(args==NULL) {
    res->v.string = NULL;
    return;
  }
  url = xmls_new(100);
  xmls_add_str(url,"/static/");

  xf_concat(pctx,locals,args,current,&rv);
  str = rval2string(&rv);
  if(str) {
    rev = xsl_get_global_key(pctx, "prefix",NULL);
    if(rev) {
      xmls_add_str(url,rev);
      xmls_add_char(url,'/');
    }
    xmls_add_str(url,str);
    rev = xsl_get_global_key(pctx, "staticFileRevisions",str);
    if(rev) {
      xmls_add_char(url,'?');
      xmls_add_str(url,rev);
    }
  }

  res->v.string = xmls_detach(url);
}

void xf_banner(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res) // XXX stub for banner!
{
  res->type = VAL_STRING;
  res->v.string = NULL;//strdup("<table bgcolor='#99FF00' width='100%' height='100%'><tr><td>banner</td></tr></table>\n");
}

void xf_exnodeset(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;

  res->type = VAL_NODESET;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  if(rv.type == VAL_NODESET) {
    res->v.nodeset = rv.v.nodeset;
    rv.v.nodeset = NULL;
  } else if(rv.type == VAL_STRING) {
    res->v.nodeset = xml_parse_string(pctx->gctx, rv.v.string, 1);
  }else {
    res->v.nodeset = NULL;
  }
  rval_free(&rv);
}

void xf_node(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  unsigned int p = 0;

  res->type = VAL_NODESET;
  res->v.nodeset = add_to_selection(NULL,current, &p);
}

void xf_key(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_NODESET;
  res->v.nodeset = NULL;
  
  RVALUE rv;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  char *key_name = rval2string(&rv);
  if(!key_name) {
    error("xf_key:: key name is NULL");
    return;
  }

  if (args->next == NULL) {
    error("xf_key:: syntax error");
    return;
  }

  xpath_execute_scalar(pctx, locals, args->next, current, &rv);
  char *key_value = rval2string(&rv);
  if(!key_value) {
    error("xf_key:: key value is NULL");
    return;
  }
  
  XMLNODE *key = pctx->keys;
  while (key != NULL) {
    if (strcmp(key->name, key_name) == 0) {
      break;
    }
    key = key->next;
  }
  if (key == NULL) {
    error("xf_key:: unknown key name: %s", key_name);
    return;
  }

  debug("xf_key:: key: %s, value: %s", key_name, key_value);

  char *format = key->content;
  int size = snprintf(NULL, 0, format, key_value);
  if (size > 0) {
    int buffer_size = size + 1;
    char *buffer = memory_allocator_new(buffer_size);
    if (snprintf(buffer, buffer_size, format, key_value) == size) {
      debug("xf_key:: key predicate: %s", buffer);
      XPATH_EXPR *expression = xpath_find_expr(pctx, buffer);
      res->v.nodeset = xpath_eval_selection(pctx, locals, current, expression);
    }
  }
}

/*******************************************************************************/

void xf_stub(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_NULL;
}

/*
 * internal:
 * Adds a function to context storage
 *
 */


void add_function(TRANSFORM_CONTEXT *pctx, char *fname, void (*fun)(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res))
{
  if(!pctx->functions) {
    pctx->cb_max = 20;
    pctx->cb_ptr = 0;
    pctx->functions = malloc(sizeof(CB_TABLE)*pctx->cb_max);
  } else if(pctx->cb_ptr >= pctx->cb_max) {
    pctx->cb_max += 20;
    pctx->functions = realloc(pctx->functions, sizeof(CB_TABLE)*pctx->cb_max);
  }
  pctx->functions[pctx->cb_ptr].name = fname;
  pctx->functions[pctx->cb_ptr].func = fun;
  ++pctx->cb_ptr;
}

static void do_callback(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res, void (*fun)())
{
  char *f_arg[30];
  unsigned i;
  RVALUE rv;
  char *s;
  
  for(i=0;i<29;++i) {
    if(args==NULL)
      break;
    xpath_execute_scalar(pctx, locals, args, current, &rv);
    s = rval2string(&rv);
    if(!s)
      s = xml_strdup("");
    f_arg[i]=s;
    args=args->next;
  }
  f_arg[i]=NULL;

  s = NULL;
  if(pctx->gctx->perl_cb_dispatcher) {
    s = call_perl_function(pctx, fun, f_arg);
  }

  res->type = VAL_STRING;
  res->v.string = xml_strdup(s);
}



void xpath_call_dispatcher(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, char *fname, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  unsigned i;

  if(!pctx->functions) {  // Linear search is used for functions, sort adding by usage - more frequent first
    add_function(pctx,"ltr:url_code",xf_urlcode); // 2132!!!
    add_function(pctx,"ltr:veristat_local",xf_veristatl); // 642
    add_function(pctx,"position",xf_position); // 356
    add_function(pctx,"count",xf_count); // 288
    add_function(pctx,"chk:check_rights",xf_check); // 202
    add_function(pctx,"substring",xf_substr); // 180
    add_function(pctx,"name",xf_name); // 168
    add_function(pctx,"normalize-space",xf_normalize);  // 112
    add_function(pctx,"last",xf_last); // 128
    add_function(pctx,"string-length",xf_strlen); // 108
    add_function(pctx,"concat",xf_concat); // 92
    add_function(pctx,"number",xf_tonum); // 52
    add_function(pctx,"ltr:veristat",xf_veristat); // 68
    add_function(pctx,"format-number",xf_format); // 46
    add_function(pctx,"contains",xf_contains);  // 50
    add_function(pctx,"round",xf_round);    //46
    add_function(pctx,"ceiling",xf_ceil); // 44
    add_function(pctx,"sum",xf_sum);  // 86
    add_function(pctx,"translate",xf_translate); // 56
    add_function(pctx,"true",xf_true); // 46
    add_function(pctx,"substring-before",xf_sub_before); // 34
    add_function(pctx,"substring-after",xf_sub_after); // 16
    add_function(pctx,"false",xf_false); // 20
    add_function(pctx,"generate-id",xf_getid); // 48
    add_function(pctx,"current",xf_current); // 30

//----------- litres-specific

    add_function(pctx,"floor",xf_floor);  // 10
    add_function(pctx,"key",xf_key); // 10
    add_function(pctx,"ltr:str_escape",xf_strescape); // 8
    add_function(pctx,"text",xf_text); // 12
    add_function(pctx,"ltr:baner_code",xf_banner); // 10
    add_function(pctx,"ltr:url_encode",xf_urlenc); // 10
    add_function(pctx,"boolean",xf_tobool); // 2
    add_function(pctx,"ltr:md5_hex",xf_md5); // 2
    add_function(pctx,"document",xf_document); // 1
    add_function(pctx,"ltr:encode_base64",xf_base64); // 2
    add_function(pctx,"exsl:node-set",xf_exnodeset); // 2
    add_function(pctx,"starts-with",xf_starts); // 0
    add_function(pctx,"local-name",xf_lname); // 2
    add_function(pctx,"node",xf_node); // 0
    add_function(pctx,"string",xf_tostr); // 0 ?
    add_function(pctx,"system-property",xf_stub); // 0
    add_function(pctx,"ltr:existsOnHost",xf_exists); // 0
  }

  if(pctx->gctx->perl_cb_dispatcher) { // first try perl overrides if any
    for(i=0;i<pctx->gctx->perl_cb_ptr;++i) {
      if(strcmp(pctx->gctx->perl_functions[i].name, fname) == 0) {
        debug("xpath_call_dispatcher:: Perl callback function: %s", fname);
        do_callback(pctx, locals, args, current, res, pctx->gctx->perl_functions[i].func);
        return;
      }
    }
  }

  for(i=0;i<pctx->cb_ptr;++i) {
    if(strcmp(pctx->functions[i].name, fname) == 0) {
      debug("xpath_call_dispatcher:: internal function: %s", fname);
      (pctx->functions[i].func)(pctx, locals, args, current, res);
      return;
    }
  }
  error("xpath_call_dispatcher:: function not found %s",fname);
  res->type = VAL_NULL;
}


void register_function(XSLTGLOBALDATA *pctx, char *fname, char *(*callback)(void *,char **,void *), void (*fun)())
{
  pctx->perl_cb_dispatcher = callback;
  if(!pctx->perl_functions) {
    pctx->perl_cb_max = 20;
    pctx->perl_cb_ptr = 0;
    pctx->perl_functions = malloc(sizeof(CB_TABLE)*pctx->perl_cb_max);
  } else if(pctx->perl_cb_ptr >= pctx->perl_cb_max) {
    pctx->perl_cb_max += 20;
    pctx->perl_functions = realloc(pctx->perl_functions, sizeof(CB_TABLE)*pctx->perl_cb_max);
  }
  if(0 == xml_strcmp(fname,"ltr:url_code")) {
    pctx->perl_urlcode = fun;
  } else {
    pctx->perl_functions[pctx->perl_cb_ptr].name = xml_strdup(fname);
    pctx->perl_functions[pctx->perl_cb_ptr].func = fun;
    ++pctx->perl_cb_ptr;
  }
}