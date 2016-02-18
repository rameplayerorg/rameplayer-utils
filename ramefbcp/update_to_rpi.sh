#!/bin/bash
IP=10.0.0.16
REMOTE=root@$IP
REMOTE_FOLDER=ramefbcp/
TARGET=$REMOTE:$REMOTE_FOLDER

ssh $REMOTE "mkdir $REMOTE_FOLDER"
scp CMakeLists.txt README.md main.c debug.* infodisplay.* icon-data.h ttf.* input.* $TARGET
ssh $REMOTE "cd ramefbcp; rm ramefbcp; mkdir -p build; cd build; cmake ..; make; mv ramefbcp ..; cd ..; ls -al ramefbcp"
