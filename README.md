# TEMPer2-getdegs

GNU/Linux utility for reading temperature measurments
using RDing TEMPer2 USB device.

## Building `getdegs` and adding udev rule for TEMPer2

```bash
make && \
sudo make rules-install
```

In case the TEMPer2 was already plugged in,
physically unplug and plug it back in.

## Usage

```bash
./getdegs [-n <count>] [--sleep <num>]
```

### Output

#### stderr

Outputs warnings and error messages to stderr.
All warnings and error messages contain at least one colon character.
As long as you are using UTF-8, the byte value of the colon character is 0x3a.

Outputs header to stderr exactly once. Header does not contain any colons.
Header includes non-ASCII characters. As long as you're using UTF-8,
no 0x3a-bytes will occur in the header.

#### stdout

Outputs timestamped measurments to stdout.
Output is tab-delimited with additional leading space characters (0x20)
for right-alignment of numbers.

### Example

```bash
./getdegs -n 5 --sleep 10
```

```
WARN: Device already detached
WARN: Device already detached
Time [s]        Internal [°C]   External [°C]
1453332865             +33.44          +15.75
1453332875             +33.44          +15.81
1453332885             +33.38          +15.75
1453332895             +33.31          +15.75
1453332905             +33.31          +15.75
```
