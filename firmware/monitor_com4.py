import serial, time, sys
port = 'COM4'
baud = 115200
try:
    ser = serial.Serial(port, baudrate=baud, timeout=0.1)
except Exception as e:
    print('ERROR opening', port, e)
    sys.exit(1)
print(f'Reading 5 seconds from {port} at {baud} baud')
end = time.time() + 5
while time.time() < end:
    try:
        line = ser.readline()
    except Exception as e:
        print('ERROR reading', e)
        break
    if line:
        try:
            print(line.decode('utf-8', errors='replace').rstrip())
        except Exception:
            print(repr(line))
ser.close()
print('Finished')
