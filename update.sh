#!/bin/bash

git pull
if [ $? -eq 0 ]
then
	arduino-cli upload --verbose -p /dev/serial0 --fqbn arduino:avr:pro volvo_rti_android
fi
