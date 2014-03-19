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

#include "ltr_xsl.h"
#include "md5.h"


void    xf_strescape(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
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
    free(mode);
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
    free(str);
  } else {
    res->v.string = str;
  }
}

void    xf_getid(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
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
    sprintf(buf,"id%x",current->uid);
    res->type = VAL_STRING;
    res->v.string = strdup(buf);
  } else {
    res->type = VAL_NULL;
  }
}

void    xf_current(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  XMLNODE *set;
  unsigned p = 0;

  set = add_to_selection(NULL,current,&p);
  res->type = VAL_NODESET;
  res->v.nodeset = set;
}

void    xf_true(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_BOOL;
  res->v.integer = 1;
}

void    xf_false(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_BOOL;
  res->v.integer = 0;
}

void    xf_last(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_INT;
  res->v.integer = 1;
  if(current) {
    for(;current->next;current=current->next)
	;
    res->v.integer = current->position;
  }
}

void    xf_concat(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  char *s = NULL;
  char *t;
  for(;args;args=args->next) {
    xpath_execute_scalar(pctx, locals, args, current, &rv);
    if(s) {
      t = rval2string(&rv);
      if(t) {
        s = realloc(s, strlen(s)+strlen(t)+3);
        strcat(s,t);
        free(t);
      }
    } else {
      s = rval2string(&rv);
    }
  }  
  res->type = VAL_STRING;
  res->v.string = s;
}

void    xf_substr(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  char *s = NULL;
  char *p;
  char *d;
  int n;
  res->type = VAL_STRING;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  s = rval2string(&rv);
  if(!s) {
    res->v.string = NULL;
    return;
  }
  xpath_execute_scalar(pctx, locals, args->next, current, &rv);
  n = (int)floor(rval2number(&rv));
  for(p=s;n>1;++p) {
    if(*p==0)
      break;
    if((0x0C0 & *p)==0x080)
        continue;
    --n;
  }
  if(args->next->next) {
    xpath_execute_scalar(pctx, locals, args->next->next, current, &rv);
    n = (int)floor(rval2number(&rv));
  } else n = 100500;
  for(d=s;n>0;--n) {
    *d++ = *p;
    if(*p++==0)
      break;
    if((0x0C0 & *p)==0x080)
      ++n;
  }
  *d = 0;

  res->v.string = s;
}

void    xf_tostr(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  char *s;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  s = rval2string(&rv);
  res->type = VAL_STRING;
  res->v.string = s;
}

void    xf_tonum(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  double d;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  d = rval2number(&rv);
  res->type = VAL_NUMBER;
  res->v.number = d;
}

void    xf_tobool(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  int d;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  d = rval2bool(&rv);
  res->type = VAL_BOOL;
  res->v.integer = d;
}

void    xf_round(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  double d;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  d = rval2number(&rv);
  res->type = VAL_NUMBER;
  res->v.number = floor(d+0.5);
}

void    xf_floor(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  double d;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  d = rval2number(&rv);
  res->type = VAL_NUMBER;
  res->v.number = floor(d);
}

void    xf_ceil(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  double d;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  d = rval2number(&rv);
  res->type = VAL_NUMBER;
  res->v.number = ceil(d);
}

void    xf_sum(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  double d = 0;
  while(args) {
    xpath_execute_scalar(pctx, locals, args, current, &rv);
    d = d + rval2number(&rv);
    args = args->next;
  }
  res->type = VAL_NUMBER;
  res->v.number = d;
}

void    xf_contains(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
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
//fprintf(stderr,"contains(%s,%s)=%d\n",s,p,res->v.integer);
  free(s);
  free(p);
}

void    xf_starts(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
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
//fprintf(stderr,"contains(%s,%s)=%d\n",s,p,res->v.integer);
  free(s);
  free(p);
}

void    xf_sub_before(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
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
  free(p);
}

void    xf_sub_after(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
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
      strcpy(s,z);
      res->v.string = s;
    }
  }
  free(p);
}

void    xf_count(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  unsigned n = 0;
  RVALUE rv;
  XMLNODE *selection;
  if(args->type == XPATH_NODE_ALL) {
    for(;current;current=current->next)
      ++n;
  } else {
    xpath_execute_scalar(pctx, locals, args, current, &rv);
    if(rv.type == VAL_NODESET) {
      for(selection=rv.v.nodeset;selection;selection=selection->next) {
        if(selection->children)
          ++n;
      }
    }
    rval_free(&rv);
  }
  res->type = VAL_INT;
  res->v.integer = n;
}

void    xf_normalize(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
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

void    xf_strlen(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  unsigned char *s, *p;
  int i;http://zx.pk.ru/forumdisplay.php?f=49

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
  free(s);  
}

void    xf_format(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  double num;
  char *pat;
  char *decf=NULL;
  char fmt_buf[200];
  char *fmt;
  rv.type = VAL_NULL;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  num = rval2number(&rv);
  xpath_execute_scalar(pctx, locals, args->next, current, &rv);
  pat = rval2string(&rv);
  if(args->next->next) {
    xpath_execute_scalar(pctx, locals, args->next->next, current, &rv);
    decf = rval2string(&rv);
  }

  if(strchr(pat,',')) {
    if(strstr(pat,".00"))
      fmt = "%02f";
    else if(strstr(pat,",0"))
      fmt = "%.01f";
    else if(strstr(pat,",##"))
      fmt = "%.2f";
    else if(strstr(pat,",#"))
      fmt = "%.1f";
    else fmt = "%f";
  } else {
    fmt = "%.0f";
  }

  sprintf(fmt_buf,fmt,num);
  res->type=VAL_STRING;
  res->v.string=strdup(fmt_buf);

//fprintf(stderr,"format(%g,'%s','%s')\n",num,pat,decf);
  free(pat);
  free(decf);
}

void    xf_translate(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  char *src,*pat,*sub;
  unsigned i;
  RVALUE rv;

  pat = sub = NULL;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  src = rval2string(&rv);
  if(args->next) {
    xpath_execute_scalar(pctx, locals, args->next, current, &rv);
    pat = rval2string(&rv);
    if(args->next->next) {
      xpath_execute_scalar(pctx, locals, args->next->next, current, &rv);
      sub = rval2string(&rv);
    }      
  }

  res->type = VAL_STRING;
  res->v.string = src;

  free(pat);
  free(sub);
}

void    xf_name(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
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
    s = strdup(current->name);
  else
    s = strdup("");

  res->type = VAL_STRING;
  res->v.string = s;
  rval_free(&rv);
}

void    xf_lname(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  char *s;
  if(current && current->name) {
    s = strchr(current->name,':');
    if(s) {
      s = strdup(s+1);
    } else {
      s = strdup(current->name);
    }
  }
  else
    s = strdup("");

  res->type = VAL_STRING;
  res->v.string = s;
}

void    xf_position(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_INT;
  res->v.integer = current->position;
}

void    xf_document(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  XMLNODE *doc = NULL;
  char *docname;
  char *abs;
  unsigned p = 0;

  xpath_execute_scalar(pctx, locals, args, current, &rv);
  docname = rval2string(&rv);
  if(docname[0]==0) {
    doc = add_to_selection(NULL,pctx->stylesheet,&p);
  } else {
    abs = get_abs_name(pctx, docname);
    if(abs) {
      doc = XMLParseFile(pctx->gctx, abs);
      doc = add_to_selection(NULL,doc,&p);
    }
  }
  if(doc) {
    res->type = VAL_NODESET;
    res->v.nodeset = doc;
  } else {
    res->type = VAL_NULL;
  }

  free(docname);
}

void    xf_text(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
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


void    xf_check(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
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
        free(rights);
      }
      free(str);
    }
  }
}

void    xf_exists(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_INT;
  res->v.integer = 1;
}

void    xf_md5(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
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
      free(str);
    }
  }
  str = xmls_detach(msg);
  md5_buffer(str,strlen(str),md5sig);
  md5_sig_to_string(md5sig,md5hex,33);
  free(str);
  res->type = VAL_STRING;
  res->v.string = strdup(md5hex);
}

void    xf_base64(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_STRING;
  res->v.string = strdup("Aa3b24455632+30h");
}

static char *url_get_specials(TRANSFORM_CONTEXT *pctx, char *name)
{
  return NULL;
}

void    xf_urlcode(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  XMLSTRING url,key;
  char *str;
  char *p;
  char *arg;
  char separ='?';
  char *value;

  res->type = VAL_STRING;
  if(args==NULL) {
    res->v.string = strdup("/");
    return;
  }

  if(pctx->gctx->perl_urlcode) {
    XMLNODE *parg;
    char *p_args[2];
    key = xmls_new(100);
    for(parg=args;parg;parg=parg->next) {
      xpath_execute_scalar(pctx, locals, parg, current, &rv);
      str = rval2string(&rv);
      xmls_add_str(key,str);
      free(str);
      if(parg->next)
        xmls_add_char(key,',');
    }
    p = xmls_detach(key);
    str = hash(p,-1,0);
    free(p);
    p = dict_find(pctx->gctx->urldict,str);
    if(!p) {
      p_args[0] = str;
      p_args[1] = NULL;
      p = (pctx->gctx->perl_cb_dispatcher)(pctx->gctx->perl_urlcode, p_args);
      dict_add(pctx->gctx->urldict,str,p);
    }
    res->v.string = strdup(p);
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
      free(str);
    } else {
      xmls_add_str(url,str);
      free(str);
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
            free(value);
            separ = '&';
          }
          free(str);
        }
      }
    }
  }

  if(separ=='?') // add trailer / for urls with no args XXX - not necessary! just for ltr:url_code compatability
    xmls_add_char(url,'/');

  res->v.string = xmls_detach(url);
}

void    xf_veristat(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
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
      free(rev);
    }
    free(str);
  }

  res->v.string = xmls_detach(url);
}

void    xf_urlenc(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;
  XMLSTRING url;
  char *str,*tofree;
  char c;

  res->type = VAL_STRING;
  if(args==NULL) {
    res->v.string = NULL;
    return;
  }
  url = xmls_new(100);
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  tofree = str = rval2string(&rv);
  for(;c=*str;++str) {
    if(c=='?')
      xmls_add_str(url,"%3F");
    else if(c==' ')
      xmls_add_str(url,"%20");
    else if(c=='&')
      xmls_add_str(url,"%26");
    else
      xmls_add_char(url,c);
  }
  free(tofree);
  res->v.string = xmls_detach(url);
}

void    xf_veristatl(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
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
      free(rev);
    }
    xmls_add_str(url,str);
    rev = xsl_get_global_key(pctx, "staticFileRevisions",str);
    if(rev) {
      xmls_add_char(url,'?');
      xmls_add_str(url,rev);
      free(rev);
    }
    free(str);
  }

  res->v.string = xmls_detach(url);
}

void    xf_banner(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res) // XXX stub for banner!
{
  res->type = VAL_STRING;
  res->v.string = NULL;//strdup("<table bgcolor='#99FF00' width='100%' height='100%'><tr><td>banner</td></tr></table>\n");
}

void    xf_exnodeset(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  RVALUE rv;

  res->type == VAL_NODESET;
  xpath_execute_scalar(pctx, locals, args, current, &rv);
  if(rv.type == VAL_NODESET) {
    res->v.nodeset = rv.v.nodeset;
    rv.v.nodeset = NULL;
  } else {
    res->v.nodeset = NULL;
  }
  rval_free(&rv);
}

void    xf_node(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  int p = 0;

  res->type == VAL_NODESET;
  res->v.nodeset = add_to_selection(NULL,current, &p);
}

void    xf_key(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type == VAL_NODESET;
  res->v.nodeset = NULL;
}

/*******************************************************************************/

void    xf_stub(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res)
{
  res->type = VAL_NULL;
}

/*
 * internal:
 * Adds a function to context storage
 *
 */


void  add_function(TRANSFORM_CONTEXT *pctx, char *fname, void (*fun)(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *args, XMLNODE *current, RVALUE *res))
{
  if(!pctx->functions) {
    pctx->cb_max = 20;
    pctx->cb_ptr = 0;
    pctx->functions = malloc(sizeof(CB_TABLE)*pctx->cb_max);
  } else if(pctx->cb_ptr >= pctx->cb_max) {
    pctx->cb_max += 20;
    pctx->functions = realloc(pctx->functions, sizeof(CB_TABLE)*pctx->cb_max);
  }
  pctx->functions[pctx->cb_ptr].name = hash(fname,-1,0);
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
      s = strdup("");
    f_arg[i]=s;
    args=args->next;
  }
  f_arg[i]=NULL;

  s = NULL;
  if(pctx->gctx->perl_cb_dispatcher) {
    s = (pctx->gctx->perl_cb_dispatcher)(fun, f_arg);
  }

  for(i=0;f_arg[i];++i) {
    free(f_arg[i]);
  }
  res->type = VAL_STRING;
  res->v.string = xml_strdup(s);
}



void    xpath_call_dispatcher(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, char *fname, XMLNODE *args, XMLNODE *current, RVALUE *res)
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
      if(pctx->gctx->perl_functions[i].name == fname) {
        do_callback(pctx, locals, args, current, res, pctx->gctx->perl_functions[i].func);
        return;
      }
    }
  }

  for(i=0;i<pctx->cb_ptr;++i) {
    if(pctx->functions[i].name == fname) {
      (pctx->functions[i].func)(pctx, locals, args, current, res);
      return;
    }
  }
fprintf(stderr, "function not found %s()\n",fname);
  res->type = VAL_NULL;
}


void register_function(XSLTGLOBALDATA *pctx, char *fname, char *(*callback)(void (*fun)(),char **args), void (*fun)())
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
    pctx->perl_functions[pctx->perl_cb_ptr].name = hash(fname,-1,0);
    pctx->perl_functions[pctx->perl_cb_ptr].func = fun;
    ++pctx->perl_cb_ptr;
  }
}