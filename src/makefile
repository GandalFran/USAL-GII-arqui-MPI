N_PROCESS=4

decrypt: decrypt.c
	mpicc -g decrypt.c -o decrypt -lcrypt

multipleRun: decrypt
	sh utils.sh 2

prepareHosts:
	sh utils.sh 1
clean:
	rm decrypt
