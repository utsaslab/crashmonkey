#!/bin/sh

echo "Exporting FSSTRESS"

cd code/fsstress
g++ fsstress.o -lattr -laio -lpthread ../../build/user_tools/src/actions.o ../../build/utils/communication/ClientCommandSender.o ../../build/utils/communication/ClientSocket.o ../../build/utils/communication/BaseSocket.o ../../build/utils/communication/ServerSocket.o -o fsstress

export FSSTRESS="../code/fsstress/fsstress"

cd ../..
