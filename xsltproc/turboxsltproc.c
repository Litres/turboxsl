#include <stdio.h>
#include <stdlib.h>

#include <ltr_xsl.h>

int main(int n, char *args[]) {
    if (n < 5) {
        fprintf(stderr, "Usage: xsltproc <threads> <stylesheet.xsl> <xml.xml> <output>\n");
        exit(1);
    }

    XSLTGLOBALDATA *gctx = XSLTInit();
    if (!gctx) {
        fprintf(stderr, "error in style processing\n");
        exit(1);
    }

    unsigned int threads = (unsigned int) atoi(args[1]);
    if (threads > 8) {
        fprintf(stderr, "maximum number of threads is 8\n");
        exit(1);
    }
    gctx->nthreads = threads;

    TRANSFORM_CONTEXT *pctx = XSLTNewProcessor(gctx, args[2]);
    if (!pctx) {
        fprintf(stderr, "error in style processing\n");
        exit(1);
    }

    XMLNODE *document = XMLParseFile(gctx, args[3]);
    if (!document) {
        fprintf(stderr, "error in document parsing\n");
        exit(1);
    }

    XMLNODE *result = XSLTProcess(pctx, document);
    if (!result) {
        fprintf(stderr, "error in transformation\n");
        exit(1);
    }

    XMLOutputFile(pctx, result, args[4]);

    XMLFreeDocument(result);
    XMLFreeDocument(document);
    XSLTFreeProcessor(pctx);
    XSLTEnd(gctx);

    return 0;
}
