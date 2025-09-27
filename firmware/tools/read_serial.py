import serial
import time
import sys

port = sys.argv[1] if len(sys.argv) > 1 else 'COM4'
baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

def read_for_seconds(port, baud, seconds=5):
    with serial.Serial(port, baud, timeout=0.1) as ser:
        end = time.time() + seconds
        while time.time() < end:
            data = ser.read(1024)
            if data:
                print(data.decode(errors='replace'), end='')

if __name__ == '__main__':
    read_for_seconds(port, baud, 5)
