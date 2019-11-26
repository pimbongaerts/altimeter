#!/usr/bin/env python
"""
Raspberry Pi Altimeter script
"""
import time
import sys
import subprocess
import pifacecad
import random
from brping import Ping1D

__author__ = 'Pim Bongaerts'
__copyright__ = 'Copyright (C) 2017 Pim Bongaerts'
__license__ = 'GPL'

REFRESH_RATE = 0.5
MESSAGE_DURATION = 5
PING_BAUDRATE = 115200
GET_IP_CMD = "hostname --all-ip-addresses"

class LCD(object):
    """ Abstraction of the PiFace Control and Display module """

    def __init__(self):
        self.piface = pifacecad.PiFaceCAD()
        self.piface.lcd.backlight_on()
        self.piface.lcd.blink_off()
        self.piface.lcd.cursor_off()
        self.piface.lcd.clear()

    def print_left_altitude(self, altitude):
        self.piface.lcd.set_cursor(0,0)
        self.piface.lcd.write('{:5.2f}m'.format(altitude))

    def print_left_depth(self, depth):
        self.piface.lcd.set_cursor(0,1)
        self.piface.lcd.write('{:5.2f}m'.format(depth))

    def print_right_altitude(self, altitude):
        self.piface.lcd.set_cursor(10,0)
        self.piface.lcd.write('{:5.2f}m'.format(altitude))

    def print_right_depth(self, depth):
        self.piface.lcd.set_cursor(10,1)
        self.piface.lcd.write('{:5.2f}m'.format(depth))

    def print(self, text):
        """ Print text to PiFace CAD """
        self.piface.lcd.clear()
        self.piface.lcd.write(text)

    def listen_for_button_presses(self):
        if self.piface.switches[0].value == 1:
            # Show IP address
            self.print('IP: {0}'.format(get_my_ip()))
            time.sleep(MESSAGE_DURATION)
            self.piface.lcd.clear()

class PingSonar(object):
    """ Abstraction of the BlueRobotics Ping sonar """

    def __init__(self, ping_descriptor):
        self.ping = Ping1D(ping_descriptor, PING_BAUDRATE)
        if self.ping.initialize() is False:
            lcd.print("Failed to initialize Ping!")
            exit(1)

    def get_distance(self):
        data = self.ping.get_distance()
        if data:
            altitude = data['distance'] / 1000
            confidence = data['confidence']
            return altitude, confidence
        else:
            altitude = "Error"
            confidence = "Error"

class PressureSensor(object):
    """ Abstraction of the BlueRobotics BAR30 pressure sensor """

    def __init__(self):
        # Emulator until depth sensor is installed
        self.depth = 20.00

    def get_depth(self):
        # Emulator until depth sensor is installed
        self.depth = 20.00 + random.uniform(-0.10, 0.10)
        return self.depth

    def get_bottom_depth(self, altitude):
        self.depth = 20.00 + random.uniform(-0.10, 0.10)
        return self.depth - altitude

def run_cmd(cmd):
    return subprocess.check_output(cmd, shell=True).decode('utf-8')

def get_my_ip():
    return run_cmd(GET_IP_CMD)[:-1]

def main():
    # Initialize screen
    lcd = LCD()

    # Initialize sensors
    ping_left = PingSonar('/dev/ttyUSB0')
    ping_right = PingSonar('/dev/ttyUSB0') # TODO: replace with second sonar
    pressure_left = PressureSensor()
    pressure_right = PressureSensor()

    # Read out sonar distances
    while True:
        # Obtain and print left altitude
        altitude_left, confidence_left = ping_left.get_distance()
        lcd.print_left_altitude(altitude_left)
        # Obtain and print left depth
        depth_left = pressure_left.get_bottom_depth(altitude_left)
        lcd.print_left_depth(depth_left)
        # Obtain and print right altitude
        altitude_right, confidence_right = ping_right.get_distance()
        lcd.print_right_altitude(altitude_right)
        # Obtain and print right depth
        depth_right = pressure_right.get_bottom_depth(altitude_right)
        lcd.print_right_depth(depth_right)
        # Listen for button presses
        lcd.listen_for_button_presses()
        # Wait length of refresh_rate before updating
        time.sleep(REFRESH_RATE)

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print('Killed by user')
        sys.exit(0)
