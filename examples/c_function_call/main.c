#include <stdio.h>
#include <stdlib.h>

extern int add(int, int);

int main(int argc, char **argv)
{
    /*
    * With command line arg example, call function implemented in onother file(calc.c)
    * Comment lines are not count as executable lines.
    */
    int a, b;
    int result = 0;

    if (argc < 3) {
        // must input 2 number args
        fprintf(stderr, "input 2 numbers for calc add.\n");
        fprintf(stderr, "Usage) ./a.out 1 2\n");
        return -1;
    }

    a = atoi(argv[1]);
    b = atoi(argv[2]);

    result = add(a, b);

    if (result < 10) {
        printf("a + b = %d, the answer is smaller than 10!\n", result);
    } else {
        printf("a + b = %d, the answer is greater than or equal to 10!\n", result);
    }

    return 0;
}
