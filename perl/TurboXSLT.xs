/* $Id$ */
/*
 * This is free software, you may use it and distribute it under the same terms as
 * Perl itself.
 *
 * Copyright 2001-2009 AxKit.com Ltd.
*/

#ifdef __cplusplus
extern "C" {
#endif
#include <turboxsl/turboxsl.h>
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"
#ifdef __cplusplus
}
#endif


MODULE = TurboXSLT   PACKAGE = TurboXSLT   PREFIX = xslt_

XMLNODE *
xslt__parse_str(gctx,str)
  XSLTGLOBALDATA *gctx
  char *str
  CODE:
  RETVAL = XMLParse(gctx,str);
  OUTPUT:
  RETVAL

XMLNODE *
xslt__parse_file(gctx,str)
  XSLTGLOBALDATA *gctx
  char *str
  CODE:
  RETVAL = XMLParseFile(gctx,str);
  OUTPUT:
  RETVAL

char *
xslt__output_str(gctx,doc)
  TRANSFORM_CONTEXT *gctx
  XMLNODE *doc
  CODE:
  RETVAL = XMLOutput(gctx,doc);
  OUTPUT:
  RETVAL

void
xslt__output_file(gctx,doc,filename)
  TRANSFORM_CONTEXT *gctx
  XMLNODE *doc
  char *filename
  CODE:
  XMLOutputFile(gctx,doc,filename);

void
setvarg(gctx,name,val)
  XSLTGLOBALDATA *gctx
  char *name
  char *val
CODE:
  set_global_var(gctx,name,val);


MODULE = TurboXSLT   PACKAGE = XSLTGLOBALDATAPtr   PREFIX = gctx_

PROTOTYPES: DISABLE

XSLTGLOBALDATA *
gctx_new(package)
  char *package
  CODE:
  RETVAL = XSLTInit();
  OUTPUT:
  RETVAL

void
gctx_DESTROY(gctx)
  XSLTGLOBALDATA *gctx
  CODE:
  XSLTEnd(gctx);


MODULE = TurboXSLT   PACKAGE = TRANSFORM_CONTEXTPtr   PREFIX = tctx_

PROTOTYPES: DISABLE

TRANSFORM_CONTEXT *
tctx_new(package,gctx,filename)
  char *package
  XSLTGLOBALDATA *gctx
  char *filename
  CODE:
  RETVAL = XSLTNewProcessor(gctx,filename);
  OUTPUT:
  RETVAL

void
tctx_DESTROY(gctx)
  TRANSFORM_CONTEXT *gctx
  CODE:
  XSLTFreeProcessor(gctx);

XMLNODE *
tctx_Transform(self,document)
  TRANSFORM_CONTEXT *self
  XMLNODE *document
  CODE:
  RETVAL = XSLTProcess(self,document);
  OUTPUT:
  RETVAL

void
SetVariable(gctx,name,val)
  TRANSFORM_CONTEXT *gctx
  char *name
  char *val
CODE:
  set_ctx_global_var(gctx,name,val);

char *
tctx_Output(gctx,doc)
  TRANSFORM_CONTEXT *gctx
  XMLNODE *doc
  CODE:
  RETVAL = XMLOutput(gctx,doc);
  OUTPUT:
  RETVAL

void
tctx_OutputFile(gctx,doc,filename)
  TRANSFORM_CONTEXT *gctx
  XMLNODE *doc
  char *filename
  CODE:
  XMLOutputFile(gctx,doc,filename);

MODULE = TurboXSLT   PACKAGE = XMLNODEPtr   PREFIX = node_

void
node_DESTROY(node)
  XMLNODE *node
  CODE:
  XMLFreeDocument(node);
