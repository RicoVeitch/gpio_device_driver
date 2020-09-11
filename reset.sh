#!/bin/bash
make
sudo rmmod gpio_device
sudo insmod gpio_device.ko
