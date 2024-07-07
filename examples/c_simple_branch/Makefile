all:
	nasm -f elf64 regs.asm
	${CC} -g -gdwarf-4 main.c regs.o -o c_simple_branch

clean:
	rm -f c_simple_branch

