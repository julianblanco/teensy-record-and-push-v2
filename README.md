# Audio Sampling and Upload Sensor

This sensor will capture audio data on a number of channels for a set amount
of time and then sleep for a "hold period". It does this repeatedly forever.
For each recording, the data is dumped to a corresponding file on an attached
SD card and also pushed to an FTP server with the same naming scheme.

## Configuration

The sensor is configured with preprocessor definitions in `src/config.h`. Most
values are documented within the source code.

### CONFIG_CHANNEL_COUNT

The number of channels to simultaneously sample during recording. There will be
a separate channel data file saved for each channel of audio data.

### CONFIG_RECORDING_DIRECTORY

The name of the based directory under which each recording will be saved. This
is a printf-style format string with a single integer (`%d`) format specifier
for the recording ID/index.

### CONFIG_CHANNEL_PATH

The absolute path to an individual channel recording. This is a printf-style
format string which takes a string (`%s`) followed by an integer (`%d`). The
string represents the recording base directory while the integer represents
the channel index.

### CONFIG_MAC_ADDRESS

Hardware MAC address to present over the ethernet port. This is an array of
six one-byte integers.

### CONFIG_FTP_ADDRESS

The IPv4 address of the FTP server.

### CONFIG_FTP_PORT

The TCP port on which the FTP server is listening.

### CONFIG_SELF_ADDRESS

The IPv4 address to assign to the sensor itself.

### CONFIG_DNS_ADDRESS

The IPv4 address of a DNS server. This is not used, but must be specified
with *some* value. It defaults to the same as `CONFIG_SELF_ADDRESS`.

### CONFIG_SD_FAT_TYPE

The FAT type for the SD card. You should normally use `3` here.

### CONFIG_SPI_CLOCK

The SPI clock frequency to configure.

### CONFIG_NETWORK_HEAP_SIZE

The size of the network heap to use. This shouldn't be changed, but if you are
running out of memory for some reason and have RAM to spare, you can increase
this value.

### CONFIG_WATCHDOG_WARNING_TIMEOUT

The amount of time in seconds before a warning is printed to the serial port about
a pending watchdog reset trigger.

### CONFIG_WATCHDOG_RESET_TIMEOUT

The amount of time in seconds before the microcontroller will reset if no new samples
are taken.

### CONFIG_SERIAL_BAUD

The baud rate for the serial port.

### CONFIG_LED

The port which is connected to a status LED. This LED is illuminated whenever the
sensor is actively recording data.

### CONFIG_RECORDING_LENGTH

The length of the audio recording in milliseconds.

### CONFIG_HOLD_LENGTH

The amount of time to sleep between recordings.

### CONFIG_FTP_USER

A valid/existing FTP user account with write permissions.

#### CONFIG_FTP_PASSWORD

The password for the user given in `CONFIG_FTP_USER`.

### CONFIG_AUDIO_BUFFER_SIZE

Size of the internal audio queue buffer. This should not be changed.

### CONFIG_SD_CARD_ROLLOFF

When set to one, the sensor will roll the oldest recording off the SD card if/when the SD
card is full or cannnot hold more recordings.
