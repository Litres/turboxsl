#include <stdio.h>
#include <stdlib.h>

#include <ltr_xsl.h>

int main(int n, char *args[]) {
    if (n < 4) {
        fprintf(stderr, "Usage: xsltproc <stylesheet.xsl> <input.xml> <output.html>\n");
        exit(1);
    }

    XSLTGLOBALDATA *gctx = XSLTInit(NULL);
    if (!gctx) {
        fprintf(stderr, "error in style processing\n");
        exit(1);
    }

    TRANSFORM_CONTEXT *pctx = XSLTNewProcessor(gctx, args[1]);
    if (!pctx) {
        fprintf(stderr, "error in style processing\n");
        exit(1);
    }

    XMLNODE *document = XMLParseFile(gctx, args[2]);
    if (!document) {
        fprintf(stderr, "error in document parsing\n");
        exit(1);
    }

    XMLNODE *result = XSLTProcess(pctx, document);
    if (!result) {
        fprintf(stderr, "error in transformation\n");
        exit(1);
    }

    XMLOutputFile(pctx, result, args[3]);

    XMLFreeDocument(result);
    XMLFreeDocument(document);
    XSLTFreeProcessor(pctx);
    XSLTEnd(gctx);

    return 0;
}
