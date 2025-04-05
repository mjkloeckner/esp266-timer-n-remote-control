# ESP8622 Timer w/ NTP Sync and Remote Control

The idea is to have a device that turns on and off one of it's outputs at a
certain time of the day, and also having the possibility to override the timer
and toggle the output remotely

To keep the clock synchronized the device uses the Network Time Protocol (NTP)

## Timer Defined State and User Override State

### Base Timer Function

If the timer is set (`timer_enabled == 1`) then after a defined amount of
time, the current time gets compared with the timer-set values, if the current
time fits inside the interval defined by the timer then the output pin
(`MAIN_OUTPUT_PIN`) gets pulled up and keeps in that state until the current
time no longer belongs to the interval or if the user manually changes the
output state, in which case the timer deactivates

### User Override

If the user overrides the state of the output then the timer is deactivated
(`timer_enabled` is set to `0`)

## Dependencies

The project is developed using  [PlatformIO
Core](https://docs.platformio.org/en/latest/core/index.html) which does the
compiling, uploading to the board and dependency management, but the same code
can compiled (with some light changes) using the Arduino IDE.

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

## LICENSE

[MIT](./LICENSE)
