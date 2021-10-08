#!/bin/bash

git pull
if [ $? -eq 1 ]
then
	arduino-cli upload --verbose -p /dev/serial0 --fqbn arduino:avr:pro volvo_rti_android
fi
