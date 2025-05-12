#Makefile
all: cykor_shell

cykor_shell: cykor_shell.c
	gcc -o cykor_shell cykor_shell.c

clean:
	rm -f cykor_shell