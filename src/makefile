N_PROCESS=1

run: decrypt
	mpiexec -np $(N_PROCESS) ./decrypt aaTrgM6tVLhas

decrypt: decrypt.c header.h
	mpicc decrypt.c -o decrypt -lcrypt