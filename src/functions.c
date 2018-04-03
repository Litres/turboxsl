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
#include <ctype.h>

#include "ltr_xsl.h"
#include "md5.h"
#include "xsl_elements.h"

char *call_perl_function(TRANSFORM_CONTEXT *pctx, void *function, char **args)
{
  if(pthread_mutex_lock(&(pctx->lock))) {
    error("call_perl_function:: lock");
    return NULL;
  }

  char *result = (pctx->gctx->perl_cb_dispatcher)(function, args, pctx->gctx->interpreter);

  if(pthread_mutex_unlock(&(pctx->lock))) {
    error("call_perl_function:: unlock");
  }

  return result;
}

XMLSTRING url_encode(const char *url) {
  XMLSTRING result = xmls_new(100);
  for (; *url; ++url) {
    int c = *url & 0xFF;
    if (isalnum(c) || c == '-' || c == '.' || c == '_'  || c == '~') {
      xmls_add_char(result, (char)c);
    } else {
      char buffer[] = {0, 0, 0, 0};
      sprintf(buffer, "%%%02X", c);
      xmls_add_str(result, buffer);
    }
  }
  return result;
}

void xf_strescape(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_STRING;
  res->v.string = NULL;

  RVALUE rv;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  char *str = rval2string(&rv);
  if(!str) {
    return;
  }

  char *mode;
  if(args->next) {
    xpath_execute_scalar(pctx, locals, args->next, current, &rv);
    mode = rval2string(&rv);
  } else {
    mode = "js";
  }

  if(xml_strcmp(mode, "js") == 0) {
    XMLSTRING s = xmls_new(200);
    for(char *p = str; *p; ++p) {
      switch(*p) {
        case '\'':
        case '\"':
        case '\\':
        case '/':
          xmls_add_char(s, '\\');
          xmls_add_char(s, *p);
          break;

        case '\r':
        case '\n':
          xmls_add_char(s, '\\');
          xmls_add_char(s, 'n');
          break;

        default:
          xmls_add_char(s, *p);
      }
    }
    res->v.string = xmls_detach(s);
  }

  if(xml_strcmp(mode, "url") == 0) {
    res->v.string = xmls_detach(url_encode(str));
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
      if (strcmp(format->name->s, format_name) == 0) {
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
    XMLSTRING value = xml_get_attr(node, xsl_a_decimal_separator);
    if (value != NULL) decimal_separator_sign = value->s;

    value = xml_get_attr(node, xsl_a_grouping_separator);
    if (value != NULL) grouping_sign = value->s;

    value = xml_get_attr(node, xsl_a_percent);
    if (value != NULL) percent_sign = value->s;

    value = xml_get_attr(node, xsl_a_zero_digit);
    if (value != NULL) digit_zero_sign = value->s;

    value = xml_get_attr(node, xsl_a_digit);
    if (value != NULL) digit_sign = value->s;

    value = xml_get_attr(node, xsl_a_pattern_separator);
    if (value != NULL) pattern_separator_sign = value->s;

    value = xml_get_attr(node, xsl_a_infinity);
    if (value != NULL) infinity_symbol = value->s;

    value = xml_get_attr(node, xsl_a_NaN);
    if (value != NULL) nan_symbol = value->s;

    value = xml_get_attr(node, xsl_a_minus_sign);
    if (value != NULL) minus_symbol = value->s;
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

  char number_string[32];
  memset(number_string, 0, sizeof(number_string));
  unsigned int number_string_position = 0;

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
    number_string[number_string_position++] = integer_number_string[precision - i - 1];
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
    number_string[number_string_position++] = decimal_separator_sign[0];

    for (unsigned int i = 0; i < maximum_fractional_part_size + 1; i++) {
      int digit = (int)floor(pow(10, i + 1) * fractional_number) % 10;
      if (digit != 0) {
        number_string[number_string_position++] = (char)(digit + 0x30);
      } else {
        number_string[number_string_position++] = '0';
        if (i == minimum_fractional_part_size) break;
      }
    }

    // rounding
    if (number_string[number_string_position - 1] >= '5')
    {
      int has_overflow = 0;

      number_string[number_string_position - 1] = 0;
      for (int i = number_string_position - 2; i >= 0; i--)
      {
        char digit = number_string[i];
        if (digit == decimal_separator_sign[0]) continue;
        if (digit == '9')
        {
          number_string[i] = '0';
          if (i == 0) has_overflow = 1;
        }
        else
        {
          number_string[i] = (char)(digit + 1);
          break;
        }
      }

      // top digit correction
      if (has_overflow)
      {
        for (int i = number_string_position; i > 0; i--)
        {
          number_string[i] = number_string[i - 1];
        }
        number_string[0] = '1';
        precision = precision + 1;
      }
    }
    else
    {
      number_string[number_string_position - 1] = 0;
    }
  }

  for (unsigned int i = 0; i < number_string_position; i++) {
    // grouping
    if (i < precision && integer_part_grouping_position > 0 && integer_part_grouping_position < precision) {
      if ((precision - i) % integer_part_grouping_position == 0) {
        output[output_position++] = grouping_sign[0];
      }
    }
    output[output_position++] = number_string[i];
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
  XMLNODE *node = current;
  if(args) {
    RVALUE rv;
    rv.type = VAL_NULL;
    xpath_execute_scalar(pctx, locals, args, current, &rv);
    node = rv.type == VAL_NODESET ? rv.v.nodeset : NULL;
  }

  char *s;
  if(node && node->name)
    s = xml_strdup(node->name->s);
  else
    s = xml_strdup("");

  res->type = VAL_STRING;
  res->v.string = s;
}

void xf_local_name(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  XMLNODE *node = current;
  if(args) {
    RVALUE rv;
    rv.type = VAL_NULL;
    xpath_execute_scalar(pctx, locals, args, current, &rv);
    node = rv.type == VAL_NODESET ? rv.v.nodeset : NULL;
  }

  char *s;
  if(node && node->name) {
    s = strchr(node->name->s,':');
    if(s) {
      s = xml_strdup(s+1);
    } else {
      s = xml_strdup(node->name->s);
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
    abs = get_absolute_path(pctx->stylesheet, docname);
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
  res->type = VAL_BOOL;
  res->v.integer = 0;

  if(pctx->user_rights == NULL)
  {
    error("xf_check:: user rights not defined");
    return;
  }

  if(args) {
    RVALUE rv;
    xpath_execute_scalar(pctx, locals, args, current, &rv);
    char *str = rval2string(&rv);
    if(str) {
      if(dict_find(pctx->user_rights, xmls_new_string_literal(str)) != NULL) res->v.integer = 1;
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
  res->type = VAL_STRING;
  if(args == NULL) {
    debug("xf_urlcode:: no arguments");
    res->v.string = "/";
    return;
  }

  if(pctx->gctx->perl_urlcode == NULL) {
    res->v.string = "/";
    return;
  }

  XMLSTRING buffer = xmls_new(100);
  for(XMLNODE *parg=args;parg;parg=parg->next) {
    RVALUE rv;
    xpath_execute_scalar(pctx, locals, parg, current, &rv);
    char *str = rval2string(&rv);
    if(str != NULL && str[0] == '-') {
      // search for parameter without lead prefix
      char *name = str + 1;
      char *value = dict_find(pctx->url_code_parameters, xmls_new_string_literal(name));
      if(value != NULL) {
        xmls_add_str(buffer, name);
        xmls_add_char(buffer,',');
        xmls_add_str(buffer, value);
        if(parg->next) xmls_add_char(buffer,',');
      }
    } else {
      xmls_add_str(buffer, str);
      if(parg->next) xmls_add_char(buffer,',');
    }
  }

  if(buffer->len == 0) {
    debug("xf_urlcode:: key is empty, return default value");
    res->v.string = "/";
    return;
  }

  char *key = xmls_detach(buffer);
  char *p = concurrent_dictionary_find(pctx->gctx->urldict, key);
  if(!p) {
    char *cache_key = key;
    if(pctx->cache_key_prefix != NULL) {
      size_t prefix_length = strlen(pctx->cache_key_prefix);
      size_t key_length = strlen(key);
      char *t = memory_allocator_new(prefix_length + key_length + 1);
      memcpy(t, pctx->cache_key_prefix, prefix_length);
      memcpy(t + prefix_length, key, key_length);
      cache_key = t;
    }

    p = external_cache_get(pctx->gctx->cache, cache_key);
    if(!p) {
      char *p_args[2];
      p_args[0] = key;
      p_args[1] = NULL;
      p = call_perl_function(pctx, pctx->gctx->perl_urlcode, p_args);
      external_cache_set(pctx->gctx->cache, cache_key, p);
    }

    int allocator_activated = memory_allocator_activate_mode(MEMORY_ALLOCATOR_MODE_GLOBAL);
    char *key_copy = xml_strdup(key);
    char *p_copy = xml_strdup(p);
    concurrent_dictionary_add(pctx->gctx->urldict, key_copy, p_copy);
    if (allocator_activated) memory_allocator_activate_mode(MEMORY_ALLOCATOR_MODE_SELF);
  }

  res->v.string = xml_strdup(p);
}

char *create_veristat_url(TRANSFORM_CONTEXT *pctx, XMLSTRING url)
{
  const char *revision = dict_find(pctx->gctx->revisions, url);
  //
  size_t revision_length = revision == NULL ? 0 : strlen(revision) + 1;

  const char *static_prefix = "/static/";
  size_t static_prefix_length = strlen(static_prefix);

  XMLSTRING result = xmls_new(static_prefix_length + url->len + revision_length);
  xmls_add_str(result, static_prefix);
  xmls_add_str(result, url->s);
  if(revision_length > 0) {
    xmls_add_str(result, "?");
    xmls_add_str(result, revision);
  }

  return xmls_detach(result);
}

void xf_veristat(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_STRING;
  if(args==NULL) {
    res->v.string = NULL;
    return;
  }

  RVALUE rv;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  char *url = rval2string(&rv);
  if(url == NULL) {
    error("xf_veristat:: wrong argument");
    res->v.string = NULL;
    return;
  }

  res->v.string = create_veristat_url(pctx, xmls_new_string_literal(url));
}

void xf_veristat_local(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_STRING;
  if(args==NULL) {
    res->v.string = NULL;
    return;
  }

  RVALUE rv;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  if(rv.type != VAL_STRING || rv.v.string == NULL) {
    error("xf_veristat_local:: wrong argument");
    res->v.string = NULL;
    return;
  }

  const char *url = rv.v.string;
  if(pctx->url_local_prefix != NULL) {
    size_t prefix_length = strlen(pctx->url_local_prefix);
    size_t url_length = strlen(url);
    char *t = memory_allocator_new(prefix_length + url_length + 1);
    memcpy(t, pctx->url_local_prefix, prefix_length);
    memcpy(t + prefix_length, url, url_length);
    url = t;
  }

  res->v.string = create_veristat_url(pctx, xmls_new_string_literal(url));
}

void xf_urlenc(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_STRING;
  res->v.string = NULL;

  if(args == NULL) {
    return;
  }

  RVALUE rv;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  char *str = rval2string(&rv);
  if(str != NULL) {
    res->v.string = xmls_detach(url_encode(str));
  }
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
  XMLNODE *tmp = NULL;
  XMLNODE *ret = NULL;
  unsigned pos = 0;
  for(XMLNODE *node=current->children;node;node=node->next) {
    tmp = add_to_selection(tmp,node,&pos);
    if(!ret) ret = tmp;
  }

  res->type = VAL_NODESET;
  res->v.nodeset = ret;
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
    if (strcmp(key->name->s, key_name) == 0) {
      break;
    }
    key = key->next;
  }
  if (key == NULL) {
    error("xf_key:: unknown key name: %s", key_name);
    return;
  }

  debug("xf_key:: key: %s, value: %s", key_name, key_value);

  char *format = key->content->s;
  int size = snprintf(NULL, 0, format, key_value);
  if (size > 0) {
    int buffer_size = size + 1;
    char *buffer = memory_allocator_new(buffer_size);
    if (snprintf(buffer, buffer_size, format, key_value) == size) {
      debug("xf_key:: key predicate: %s", buffer);
      XPATH_EXPR *expression = xpath_find_expr(pctx, xmls_new_string_literal(buffer));
      res->v.nodeset = xpath_eval_selection(pctx, locals, current, expression);
    }
  }
}

void xf_thread_id(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_STRING;
  char buffer[256];
  sprintf(buffer, "%p", pthread_self());
  res->v.string = xml_strdup(buffer);
}

void xf_localization_base(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res, int mode)
{
  res->type = VAL_STRING;
  res->v.string = NULL;

  if (pctx->localization_entry == NULL)
  {
    error("xf_localization:: localization not set");
    return;
  }

  RVALUE rv;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  char *id = rval2string(&rv);
  if (id == NULL)
  {
    error("xf_localization:: string is NULL");
    return;
  }

  XMLNODE *p = args->next;
  const char *string = NULL;

  if (mode == 0)
  {
    string = localization_entry_get(pctx->localization_entry, id);
  }
  if (mode == 1)
  {
    // skip second string argument
    p = p->next;

    // get plural value
    xpath_execute_scalar(pctx, locals, p, current, &rv);
    int n = (int)rval2number(&rv);
    string = localization_entry_get_plural(pctx->localization_entry, id, n);

    p = p->next;
  }

  if (string == NULL)
  {
    error("xf_localization:: unknown string %s", id);
    res->v.string = xml_strdup(id);
    return;
  }

  if (p == NULL)
  {
    res->v.string = xml_strdup(string);
    return;
  }

  const char *format = "{%s}";
  while (p != NULL)
  {
    xpath_execute_scalar(pctx, locals, p, current, &rv);
    const char *name = rval2string(&rv);

    p = p->next;
    xpath_execute_scalar(pctx, locals, p, current, &rv);
    const char *value = rval2string(&rv);
    
    int size = snprintf(NULL, 0, format, name);
    if (size == 0)
    {
      error("xf_localization:: allocator");
      return;
    }

    size_t buffer_size = size + 1;
    char *buffer = memory_allocator_new(buffer_size);
    if (snprintf(buffer, buffer_size, format, name) != size)
    {
      error("xf_localization:: wrong size");
      return;
    }

    XMLSTRING result = xmls_new(64);
    const char *last = string;
    char *s = strstr(last, buffer);
    while (s != NULL)
    {
      xmls_append(result, xmls_new_string(last, s - last));
      xmls_add_str(result, value);
      
      last = s + size;
      s = strstr(last, buffer);
    }

    if (*last != 0)
    {
      xmls_add_str(result, last);
    }

    string = xmls_detach(result);
    p = p->next;
  }

  res->v.string = string;
}

void xf_localization(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  xf_localization_base(pctx, locals, args, current, res, 0);
}

void xf_localization_plural(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  xf_localization_base(pctx, locals, args, current, res, 1);
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

void xpath_setup_functions(TRANSFORM_CONTEXT *pctx)
{
  add_function(pctx,"ltr:url_code",xf_urlcode); // 2132!!!
  add_function(pctx,"ltr:veristat_local", xf_veristat_local); // 642
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
  add_function(pctx,"local-name", xf_local_name); // 2
  add_function(pctx,"node",xf_node); // 0
  add_function(pctx,"string",xf_tostr); // 0 ?
  add_function(pctx,"system-property",xf_stub); // 0
  add_function(pctx,"ltr:existsOnHost",xf_exists); // 0
  add_function(pctx,"ltr:thread-id",xf_thread_id);
  add_function(pctx,"ltr:__l",xf_localization);
  add_function(pctx,"ltr:__ln",xf_localization_plural);
}

void xpath_call_dispatcher(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, char *fname, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  unsigned i;
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