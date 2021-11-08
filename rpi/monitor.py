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

DISPLAY_ACTIVE_AT = time.time()
DISPLAY_UP = False
DISPLAY_UP_TIMEOUT = 300

ANDROID_ACTIVE = False


def main() -> None:

    _thread.start_new_thread(serial_monitor, ())
    time.sleep(5)
    _thread.start_new_thread(usb_monitor, ())

    while True:
        time.sleep(1)

def serial_monitor() -> None:
    global DISPLAY_UP, DISPLAY_ACTIVE_AT, DISPLAY_UP_TIMEOUT

    logging.info('Starting serial monitor thread')
    LAST_ANDROID_ACTIVE = ANDROID_ACTIVE

    ser = None

    while True:
        try:
            if os.path.exists('/home/pi/update.lock'):
                logging.debug('Update is in progress')
                if ser is not None:
                    logging.debug('Closing serial')
                    ser.close()
                    ser = None
                logging.debug('Waiting 5 seconds')
                time.sleep(5)
                continue

            if ser is None:
                logging.debug('Opening serial')
                ser = serial.Serial(port='/dev/ttyUSB0', baudrate=115200, timeout=1)
                logging.info('Opened serial')

            line = ser.readline().strip().decode('utf-8')

            if line != '':
                logging.debug('Serial received: {0}'.format(line))

            if line == 'EVENT_RPI_REBOOT':
                subprocess.check_call('sudo shutdown -r now', shell=True)

            if line == 'EVENT_RPI_SHUTDOWN':
                subprocess.check_call('sudo shutdown -h now', shell=True)

            if line == 'EVENT_KEY_UP':
                subprocess.check_call('export DISPLAY=:0.0 && xdotool key Up', shell=True)
                continue

            if line == 'EVENT_KEY_DOWN':
                subprocess.check_call('export DISPLAY=:0.0 && xdotool key Down', shell=True)
                continue

            if line == 'EVENT_KEY_LEFT':
                subprocess.check_call('export DISPLAY=:0.0 && xdotool key 1', shell=True)
                continue

            if line == 'EVENT_KEY_RIGHT':
                subprocess.check_call('export DISPLAY=:0.0 && xdotool key 2', shell=True)
                continue

            if line == 'EVENT_KEY_ENTER':
                subprocess.check_call('export DISPLAY=:0.0 && xdotool key Return', shell=True)
                continue

            if line == 'EVENT_KEY_BACK':
                subprocess.check_call('export DISPLAY=:0.0 && xdotool key Escape', shell=True)
                continue

            if line == 'EVENT_KEY_BACK_LONG':
                subprocess.check_call('export DISPLAY=:0.0 && xdotool key h', shell=True)
                continue

            if LAST_ANDROID_ACTIVE is not ANDROID_ACTIVE and ANDROID_ACTIVE is True:
               DISPLAY_UP = True
               serial_write(ser, 'DISPLAY_UP')

            if ANDROID_ACTIVE is True:
               DISPLAY_ACTIVE_AT = time.time()

            if DISPLAY_UP is True and time.time() - DISPLAY_ACTIVE_AT >= DISPLAY_UP_TIMEOUT:
               DISPLAY_UP = False
               serial_write(ser, 'DISPLAY_DOWN')

            LAST_ANDROID_ACTIVE = ANDROID_ACTIVE

            time.sleep(0.01)

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
    global ANDROID_ACTIVE
    logging.info('Starting USB monitor thread')

    while True:
        try:
            android_found = False
            output = subprocess.check_output('lsusb', shell=True).decode('utf-8')
            for line in output.split('\n'):
                if not line:
                    continue
                if 'android' in line.lower():
                    if ANDROID_ACTIVE is False:
                        logging.info('Found android device: {0}'.format(line))
                    android_found = True
                    break
            ANDROID_ACTIVE = android_found
        except Exception as e:
            logging.error('Exception while scanning USB: {0}'.format(e))
        finally:
            time.sleep(3)


if __name__ == '__main__':
    main()
