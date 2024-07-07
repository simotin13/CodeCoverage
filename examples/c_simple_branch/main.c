#include <stdio.h>

extern void write_rax();

int main(int argc, char **argv)
{
    write_rax();
    if (argc == 2) {
        printf("argc is 2\n");
        return 1;
    }
    return 0;
}
