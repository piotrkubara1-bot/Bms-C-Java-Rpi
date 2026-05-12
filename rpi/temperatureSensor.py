#!/usr/bin/env python3
import w1thermsensor


def main():
    sensor = w1thermsensor.W1ThermSensor()
    temp = sensor.get_temperature()
    print(temp)


if __name__ == "__main__":
    main()
