N_PROCESS=5

run: decrypt
	rm *.o
	mpiexec -np $(N_PROCESS) ./decrypt

decrypt: decrypt.c toString.o mpiUtils.o utils.h dataDefinition.h
	mpicc -g decrypt.c toString.o mpiUtils.o -o decrypt -lcrypt

mpiUtils.o: mpiUtils.c mpiUtils.h utils.h dataDefinition.h
	mpicc -g -c mpiUtils.c

toString.o: toString.h toString.c dataDefinition.h
	mpicc -g -c toString.c

clean:
	rm decrypt