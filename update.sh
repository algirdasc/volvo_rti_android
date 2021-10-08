#!/bin/bash

FQBN="arduino:avr:pro"
SKETCH="volvo_rti_android"

git pull

if [ $? -eq 0 ]
then
	arduino-cli compile --verbose --fqbn $FQBN $SKETCH
fi

if [ $? -eq 0 ]
then
	arduino-cli upload --verbose -p /dev/serial0 --fqbn $FQBN $SKETCH
fi
