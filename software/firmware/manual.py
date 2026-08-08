import serial, time
ser = serial.Serial('/dev/cu.usbserial-DK0GEBC1')
ser.dtr = True
ser.rts = True
time.sleep(3)
ser.rts = False
time.sleep(0.3)
ser.dtr = False
ser.close()
