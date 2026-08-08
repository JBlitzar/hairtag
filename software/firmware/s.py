import serial, time
ser = serial.Serial('/dev/cu.usbserial-DK0GEBC1', 115200, timeout=0)
ser.dtr = False
ser.rts = False
buf = b''
end = time.time() + 3
while time.time() < end:
    data = ser.read(4096)
    if data:
        buf += data
ser.close()
print(repr(buf))
