# Linux Device Driver for Dummy GPIO Port Device
Device driver contains an interrupt handler to read a half byte at a time from a dummy GPIO port device. Device node is created under /dev/gpio_device by the driver. Interrupt is triggered when the port device writes it's half byte (alternates from most siginficant to least sigificant). The top half of the handler reassembles and stores the byte into a circular buffer. The bottom half (tasklet) reads from the buffer and stores the byte into it's page list. The device node is read only.

Data can be generated to the dummy port by ./data_generator <file1> <file2>. Each file represents a 'session'. A reader/consumer may only read one session.
