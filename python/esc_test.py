#!/usr/bin/env python

import RPi.GPIO as GPIO
import time
import gc

# Hopefully reduce interruptions
gc.disable()

# Original pin, before accident
# GPIO21
#ESC_PIN = 40

# New pin
# GPIO19
ESC_PIN = 35

# DSHOT150 TIMINGS
# T0H = 2500ns
# T0L = 4180ns
# T1H = 5000ns
# T1L = 1680ns
# Total = 6680ns

# Easily adjust all values by a fixed amount to allow for overheads
COMPENSATION = 2

T0H = (250/100000000) - COMPENSATION
T0L = (418/100000000) - COMPENSATION
T1H = (500/100000000) - COMPENSATION
T1L = (168/100000000) - COMPENSATION

INTER_PACKET_DELAY = 0.000002


def transmit_one():
    GPIO.output(ESC_PIN, GPIO.HIGH)
    time.sleep(T1H)
    GPIO.output(ESC_PIN, GPIO.LOW)
    time.sleep(T1L)


def transmit_zero():
    GPIO.output(ESC_PIN, GPIO.HIGH)
    time.sleep(T0H)
    GPIO.output(ESC_PIN, GPIO.LOW)
    time.sleep(T0L)


def inter_packet_delay():
    time.sleep(INTER_PACKET_DELAY)


def transmit_value(value, telemetry):
    # Add telemetry bit
    packet_data = (value << 1) | 1 if telemetry else 0

    # Calculate and add CRC
    crc_sum = (packet_data ^ (packet_data >> 4) ^ (packet_data >> 8)) & 0xf
    packet_data = (packet_data << 4) | crc_sum

    # Output the packet
    for bit in range(16):
        if (packet_data >> (15 - bit)) & 1:
            transmit_one()
        else:
            transmit_zero()


if __name__ == "__main__":
    # Setup GPIO
    GPIO.setmode(GPIO.BOARD)
    GPIO.setup(ESC_PIN, GPIO.OUT)

    # Send reset value to arm the ESC
    print("RESETTING")
    for i in range(20000):
        transmit_value(0)
        time.sleep(0.00001)

    # Send values from 48 (throttle = 0) to 100
    for i in range(48, 100, 1):
        throttle = i
        print("THROTTLE {}".format(throttle))
        for i in range(20000):
            transmit_value(throttle)
            inter_packet_delay()

    # Send reset again
    for i in range(100):
        transmit_value(0)
        inter_packet_delay()
