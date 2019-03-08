#!/bin/sh

MYUSER="i0918455"
NPROC=5

IP1="127.0.0.1"
IP2="172.20.2.30"
IP3="172.20.3.37"
IP4="172.20.2.172"
IP5="172.20.2.108"

if [ "$1" = 1 ];
then
	ssh-keygen -t dsa
	cat $HOME/.ssh/id_dsa.pub >> id_dsa.pub
	ssh-copy-id -i "$HOME/.ssh/id_dsa.pub" "$MYUSER@$IP2"
	ssh-copy-id -i "$HOME/.ssh/id_dsa.pub" "$MYUSER@$IP3"
	ssh-copy-id -i "$HOME/.ssh/id_dsa.pub" "$MYUSER@$IP4"
	ssh-copy-id -i "$HOME/.ssh/id_dsa.pub" "$MYUSER@$IP5"
else
	mpirun -np "$NPROC" -host "$IP1,$IP2,$IP3,$IP4,$IP5" ./decrypt
fi
