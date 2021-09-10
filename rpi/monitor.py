import serial
import _thread
import logging
import pyudev
import subprocess
import time

logging.basicConfig(
    # filename='monitor.log',
    # filemode='w',
    level=logging.DEBUG,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)


def scan_for_android(devices: list, action: str = 'found') -> bool:
    for device in devices:
        model = device.get('ID_MODEL_FROM_DATABASE')
        id_vendor = device.get('ID_VENDOR')
        id_model = device.get('ID_MODEL')
        id_serial = device.get('ID_SERIAL')
        if id_vendor is None:
            id_vendor = device.get('ID_VENDOR_FROM_DATABASE')
            id_model = 'Unknown'
            id_serial = 'Unknown'
        if model is not None and 'Android' in model:
            logging.info('{0} {1} {2} ({3})'.format(action.title(), id_vendor, id_model, id_serial))
            return True

    return False


logging.info('Scanning already plugged in USB devices')
context = pyudev.Context()
DISPLAY_ACTIVE = scan_for_android(context.list_devices(ID_BUS='usb'))
DISPLAY_TIMEOUT = 3


def main() -> None:
    logging.info('Opening serial')
    ser = serial.Serial(port='/dev/serial0', baudrate=115200, timeout=1)

    _thread.start_new_thread(usb_monitor, ())
    _thread.start_new_thread(serial_monitor, (ser,))

    while True:
        pass

    logging.info('Closing serial')
    ser.close()


def serial_monitor(ser: serial) -> None:
    logging.info('Starting serial monitor thread')
    last_display_state_ts = time.time()

    while True:
        line = ser.readline().strip()

        if line != b'':
            logging.debug('Serial received: {0}'.format(line.decode('utf-8')))

        if line == b'EVENT_SHUTDOWN':
            subprocess.check_call('sudo shutdown -h now', shell=True)

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


def serial_write(ser: serial, data: str) -> None:
    try:
        logging.debug('Serial send: {0}'.format(data))
        ser.write(data.encode('utf-8'))
        ser.write(b'\n')
        ser.flush()
    except Exception as e:
        logging.error('SERIAL: {0}'.format(e))


def usb_monitor() -> None:
    global DISPLAY_ACTIVE
    logging.info('Starting USB monitor thread')
    monitor = pyudev.Monitor.from_netlink(context)
    monitor.filter_by(subsystem='usb')
    for action, device in monitor:
        model = device.get('ID_MODEL_FROM_DATABASE')
        if model is None:
            model = device.get('PRODUCT')
        logging.debug('USB {0} {1}'.format(action.title(), model))
        if action == 'add' or action == 'remove':
            result = scan_for_android([device], action)
            if result is True:
                DISPLAY_ACTIVE = action == 'add'


if __name__ == '__main__':
    main()
