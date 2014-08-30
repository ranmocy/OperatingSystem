
all: findminmax_seq findminmax_par1 findminmax_par2

findminmax_seq: findminmax_seq.c measure.h
	gcc -O2 -o findminmax_seq findminmax_seq.c -std=c99

findminmax_par1: findminmax_par1.c measure.h
	gcc -O2 -o findminmax_par1 findminmax_par1.c -std=c99

findminmax_par2: findminmax_par2.c measure.h
	gcc -O2 -o findminmax_par2 findminmax_par2.c -std=c99

clean:
	/bin/rm -f findminmax_seq findminmax_par1 findminmax_par2
