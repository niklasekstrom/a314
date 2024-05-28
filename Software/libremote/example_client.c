#include <exec/types.h>

#include <proto/exec.h>

#include <stdio.h>

#include "proto_lib.h"

struct Library *ExampleBase;

int main()
{
    ExampleBase = OpenLibrary(EXAMPLE_LIB_NAME, 0);
    if (!ExampleBase)
    {
        printf("Unable to open " EXAMPLE_LIB_NAME "\n");
        return 0;
    }

    const char expr[] = "5 + 17 // (3 + 1)";
    printf("EvalExpr(%s) = %d\n", expr, EvalExpr(expr));

    printf("Sleep(5) = %lu\n", Sleep(5));

    CloseLibrary(ExampleBase);
    return 0;
}
