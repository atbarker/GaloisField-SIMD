CFLAGS="-mssse3"

All:
	gcc -g -mssse3 -O0 -c gf256.c
	gcc -g -O0 -c cm256.c
	gcc -g -O0 -c main.c
	gcc -g -O0 -o main main.o cm256.o gf256.o
clean:
	rm *.o main
