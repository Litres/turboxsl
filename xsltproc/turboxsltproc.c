#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "turboxsl.h"


int main(int n, char *args[])
{
  XMLNODE *result, *document;
  TRANSFORM_CONTEXT *pctx;
  XSLTGLOBALDATA *gctx;
  char *res;

  if(n<3) {
    fprintf(stderr,"Usage: xsltproc <stylesheet.xsl> <xml.xml>\n");
    exit(1);
  }

  gctx = XSLTInit();
  if(!gctx) {
    fprintf(stderr, "error in style processing\n");
    exit(1);
  }

  pctx = XSLTNewProcessor(gctx, args[1]);
  if(!pctx) {
    fprintf(stderr, "error in style processing\n");
    exit(1);
  }

  document = XMLParseFile(gctx, args[2]);
  if(!document) {
    fprintf(stderr, "error in document parsing\n");
    exit(1);
  }

  result = XSLTProcess(pctx, document);
  if(!result) {
    fprintf(stderr, "fuck-up in transformation\n");
    exit(1);
  }

  res = XMLOutput(pctx, result);
  puts(res);
  free(res);
  XMLFreeDocument(result);
  XMLFreeDocument(document);
  XSLTFreeProcessor(pctx);
  XSLTEnd(gctx);

  return 0;
}
