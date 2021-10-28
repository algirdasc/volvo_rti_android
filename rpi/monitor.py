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
ASC2PS2 = {
    '1'         : bytearray(b'\x16\xF0\x16'),
    '2'         : bytearray(b'\x1E\xF0\x1E'),
    'HOME'      : bytearray(b'\xE0\x6C\xE0\xF0\x6C'),
    'END'       : bytearray(b'\xE0\x69\xE0\xF0\x69'),
    'UP'        : bytearray(b'\xE0\x75\xE0\xF0\x75'),
    'DOWN'      : bytearray(b'\xE0\x72\xE0\xF0\x72'),
    'LEFT'      : bytearray(b'\xE0\x6B\xE0\xF0\x6B'),
    'RIGHT'     : bytearray(b'\xE0\x74\xE0\xF0\x74'),
    'ENTER'     : bytearray(b'\x5A\xF0\x5A'),
    'ESC'       : bytearray(b'\x76\xF0\x76'),
}

def send_ps2(ser, sequence):    
    l = int(len(sequence))
    f0c = 0
    for i in range(l):
        scancode = sequence[i]
        print(scancode)
        if scancode == 0xF0:
           time.sleep(0.01)
           f0c = 2
        ser.write(bytearray([scancode]))
        if f0c > 0:
           f0c -= 1
           if f0c == 0:
               time.sleep(0.01)

def main() -> None:

    ps2 = serial.Serial(port='/dev/ttyVB00', baudrate=9600, parity=serial.PARITY_NONE, timeout=5)

    _thread.start_new_thread(usb_monitor, ())
    _thread.start_new_thread(serial_monitor, (ps2))

    while True:
        pass

def serial_monitor(ps2) -> None:
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

            line = ser.readline().strip().decode('utf-8')

            if line != '':
                logging.debug('Serial received: {0}'.format(line))

            if line == 'EVENT_RPI_SHUTDOWN':
                # serial_write(ser, 'DISPLAY_DOWN')
                subprocess.check_call('sudo shutdown -h now', shell=True)
                exit()

            if line == 'EVENT_KEY_UP':
                send_ps2(ps2, ASC2PS2['UP'])

            if line == 'EVENT_KEY_DOWN':
                send_ps2(ps2, ASC2PS2['DOWN'])

            if line == 'EVENT_KEY_LEFT':
                send_ps2(ps2, ASC2PS2['1'])

            if line == 'EVENT_KEY_RIGHT':
                send_ps2(ps2, ASC2PS2['2'])

            if line == 'EVENT_KEY_ENTER':
                send_ps2(ps2, ASC2PS2['ENTER'])

            if line == 'EVENT_KEY_BACK':
                send_ps2(ps2, ASC2PS2['ESC'])
                
            if line == 'EVENT_KEY_BACK_LONG':
                send_ps2(ps2, ASC2PS2['HOME'])                                             

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
