N_PROCESS=1

decrypt: decrypt.c utils.h
	mpicc -g decrypt.c -o decrypt -lcrypt

run: decrypt
	mpirun -np $(N_PROCESS) ./decrypt

clean:
	rm decrypt
