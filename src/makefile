N_PROCESS=5

decrypt: decrypt.c toString.o mpiUtils.o utils.h dataDefinition.h
	mpicc -g decrypt.c toString.o mpiUtils.o -o decrypt -lcrypt

mpiUtils.o: mpiUtils.c mpiUtils.h utils.h dataDefinition.h
	mpicc -g -c mpiUtils.c

toString.o: toString.h toString.c dataDefinition.h
	mpicc -g -c toString.c

run: decrypt
	mpiexec -np $(N_PROCESS) ./decrypt

clean:
	rm decrypt
