# ESP8622 Timer w/ NTP Sync and Remote Control

The idea is to have a device that turns on and off one of it's outputs at a
certain time of the day, and also having the possibility to override the timer
and toggle the output remotely.

To keep the clock synchronized the device uses the Network Time Protocol (NTP)

## Dependencies

* [PlatformIO Core](https://docs.platformio.org/en/latest/core/index.html)

## Building and Uploading to the Board

Clone the repository and navigate to the folder

```console
$ git clone https://github.com/mjkloeckner/esp8266-timer.git
```

Then install the required libraries for the project

```console
$ pio pkg install
```

Compile the code 

```console
$ pio run
```

Or even better, if you have the board connected, compile and upload at the same
time

```console
$ pio run -t upload
```

Finally upload the files to the filesystem of the board

```console
$ pio run -t uploadfs
```
