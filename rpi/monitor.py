import os
import serial
import _thread
import logging
import subprocess
import time

logging.basicConfig(
    # filename='monitor.log',
    # filemode='w',
    level=logging.DEBUG,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)

DISPLAY_ACTIVE = False
DISPLAY_TIMEOUT = 3


def main() -> None:
    _thread.start_new_thread(usb_monitor, ())
    _thread.start_new_thread(serial_monitor, ())

    while True:
        pass

def serial_monitor() -> None:
    logging.info('Starting serial monitor thread')
    last_display_state_ts = time.time()

    ser = None

    while True:
        try:
            if os.path.exists('/home/pi/update.lock'):
                logging.debug('Update is in progress')
                if ser is not None:
                    logging.debug('Closing serial')
                    ser.close()
                    ser = None
                logging.debug('Waiting 30 seconds')
                time.sleep(30)
                continue

            if ser is None:                
                logging.debug('Opening serial')
                ser = serial.Serial(port='/dev/serial0', baudrate=115200, timeout=1)
                logging.info('Opened serial')

            line = ser.readline().strip()

            if line != b'':
                logging.debug('Serial received: {0}'.format(line.decode('utf-8')))

            if line == b'EVENT_RPI_SHUTDOWN':
                serial_write(ser, 'DISPLAY_DOWN')
                subprocess.check_call('sudo shutdown -h now', shell=True)
                exit()

            if line == b'EVENT_KEY_UP':
                subprocess.check_call('export DISPLAY=:0.0 && xdotool search --class autoapp key --window %@ Up', shell=True)

            if line == b'EVENT_KEY_DOWN':
                subprocess.check_call('export DISPLAY=:0.0 && xdotool search --class autoapp key --window %@ Down', shell=True)

            if line == b'EVENT_KEY_LEFT':
                subprocess.check_call('export DISPLAY=:0.0 && xdotool search --class autoapp key --window %@ 1', shell=True)

            if line == b'EVENT_KEY_RIGHT':
                subprocess.check_call('export DISPLAY=:0.0 && xdotool search --class autoapp key --window %@ 2', shell=True)

            if line == b'EVENT_KEY_ENTER':
                subprocess.check_call('export DISPLAY=:0.0 && xdotool search --class autoapp key --window %@ Return', shell=True)

            if line == b'EVENT_KEY_BACK':
                subprocess.check_call('export DISPLAY=:0.0 && xdotool search --class autoapp key --window %@ Escape', shell=True)

            if time.time() - last_display_state_ts >= DISPLAY_TIMEOUT:
                serial_write(ser, 'DISPLAY_UP' if DISPLAY_ACTIVE is True else 'DISPLAY_DOWN')
                last_display_state_ts = time.time()
        except Exception as e:
            ser = None
            logging.error('Exception while reading serial: {0}'.format(e))
            time.sleep(10)


def serial_write(ser: serial, data: str) -> None:
    try:
        logging.debug('Serial send: {0}'.format(data))
        ser.write(data.encode('utf-8'))
        ser.write(b'\n')
        ser.flush()
    except Exception as e:
        logging.error('Exception while writing to serial: {0}'.format(e))


def usb_monitor() -> None:
    global DISPLAY_ACTIVE
    logging.info('Starting USB monitor thread')    

    while True:
        try:
            android_found = False
            output = subprocess.check_output('lsusb', shell=True).decode('utf-8')
            for line in output.split('\n'):
                if not line:
                    continue
                if 'android' in line.lower():
                    if DISPLAY_ACTIVE is False:
                        logging.info('Found android device: {0}'.format(line))
                    android_found = True
                    break                    
            DISPLAY_ACTIVE = android_found
        except Exception as e:
            logging.error('Exception while scanning USB: {0}'.format(e))
        finally:
            time.sleep(3)


if __name__ == '__main__':
    main()
