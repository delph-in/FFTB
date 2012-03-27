web: web.c tree.c
	gcc -g web.c tree.c -la -o web -ltsdb -lace
