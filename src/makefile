N_PROCESS=4

run: decrypt
	mpiexec -np $(N_PROCESS) ./decrypt

decrypt: decrypt.c header.h
	mpicc decrypt.c -o decrypt -lcrypt -g

clean:
	rm log.debug
	rm decrypt