/**
 * Sensor configuration
 */
#ifndef _SENSOR_CONFIG_H_
#define _SENSOR_CONFIG_H_

// Number of audio channels to record
#define CONFIG_CHANNEL_COUNT               1
// Name of the directory to store an individual recording; formatted with a single integer
#define CONFIG_RECORDING_DIRECTORY         "/rec%d"
// Path to an individual channel recording including the recording index and the channel index
#define CONFIG_CHANNEL_PATH                "%s/chan%d.raw"
// MAC Address used for ethernet communication
#define CONFIG_MAC_ADDRESS                 {0xDE,0xAD,0xBE,0xEF,0xC0,0xDE}
//Audio server IP adderss
#define CONGIG_UDP_ADDRESS                IPAddress(192,168,42,6)
// FTP Server IP address
#define CONFIG_FTP_ADDRESS                 IPAddress(192,168,42,6)
#define CONFIG_FTP_PORT                    21
// Self-assigned IP address
#define CONFIG_SELF_ADDRESS                IPAddress(192,168,42,10)
// DNS Address (not used)
#define CONFIG_DNS_ADDRESS                 IPAddress(192,168,42,10)
//record type (SD 1, USB 2, UDP 3)
#define RECORD_TYPE 2
//internal use for code
#define SDrecord 1
#define USBrecord 2
#define UDPrecord 3
// SD Card FAT File System Type 
#define CONFIG_SD_FAT_TYPE                 3
// SD Card SS Pin is defined by the board in some cases
#ifndef SDCARD_SS_PIN
#define CONFIG_SD_CS_PIN                   SS
#else
#define CONFIG_SD_CS_PIN                   SDCARD_SS_PIN
#endif
// SPI Clock Frequency
#define CONFIG_SPI_CLOCK                   SD_SCK_MHZ(50)
// Network Heap Size
#define CONFIG_NETWORK_HEAP_SIZE           (1024*120)
// Watchdog warning timeout (seconds, 1->128)
#define CONFIG_WATCHDOG_WARNING_TIMEOUT    30
// Watchdog reset timeout (seconds, 1->128, greater than warning)
#define CONFIG_WATCHDOG_RESET_TIMEOUT      60
// Serial baud rate
#define CONFIG_SERIAL_BAUD                 9600
// LED used to indicate sampling
#define CONFIG_LED                         LED_BUILTIN
// Length of time to sample (milliseconds)
#define CONFIG_RECORDING_LENGTH               5000
// Length of time to sleep between sampling (milliseconds)
#define CONFIG_HOLD_LENGTH                 25000
// FTP credentials for sample uploads
#define CONFIG_FTP_USER                    "ftpuser"
#define CONFIG_FTP_PASSWORD                "just4munk"
// Size of the audio queue buffer
#define CONFIG_AUDIO_BUFFER_SIZE           256
// Roll off old recordings when SD card is full
#define CONFIG_SD_CARD_ROLLOFF             0


/*********************************************************

 The following blocks utilize the above configuration to
 verify some basic format or value constraints and build
 more complex configurations from the above basic configs.

*********************************************************/

#if CONFIG_SD_FAT_TYPE == 0
#define CONFIG_SD_CONTROLLER                      SdFat
#define CONFIG_SD_FILE                            FsFile
#elif CONFIG_SD_FAT_TYPE == 1
#define CONFIG_SD_CONTROLLER                      SdFat32
#define CONFIG_SD_FILE                            FsFile32
#elif CONFIG_SD_FAT_TYPE == 2
#define CONFIG_SD_CONTROLLER                      SdExFat
#define CONFIG_SD_FILE                            ExFile
#elif CONFIG_SD_FAT_TYPE == 3
#define CONFIG_SD_CONTROLLER                      SdFs
#define CONFIG_SD_FILE                            FsFile
#else
#error invalid sd fat type
#endif

#if CONFIG_WATCHDOG_WARNING_TIMEOUT < 0 || CONFIG_WATCHDOG_WARNING_TIMEOUT > 128
#error watchdog warning timeout out of bounds (expected [1,128])
#endif
#if CONFIG_WATCHDOG_RESET_TIMEOUT < 0 || CONFIG_WATCHDOG_RESET_TIMEOUT > 128
#error watchdog reset timeout out of bounds (expected [1,128])
#endif
#if CONFIG_WATCHDOG_RESET_TIMEOUT < CONFIG_WATCHDOG_WARNING_TIMEOUT
#error watchdog reset timeout must be greater than warning timeout
#endif

#if CONFIG_CHANNEL_COUNT == 1
#define CONFIG_AUDIO_PATCH_INIT AudioConnection(sine1, 0, m_audio_queue[0], 0)
#elif CONFIG_CHANNEL_COUNT == 2
#define CONFIG_AUDIO_PATCH_INIT \
  AudioConnection(m_tdm, 0, m_audio_queue[0], 0), \
  AudioConnection(m_tdm, 2, m_audio_queue[1], 0)
#elif CONFIG_CHANNEL_COUNT == 4
#define CONFIG_AUDIO_PATCH_INIT \
  AudioConnection(m_tdm, 2, m_audio_queue[0], 0), \
  AudioConnection(m_tdm, 4, m_audio_queue[1], 0), \
  AudioConnection(m_tdm, 6, m_audio_queue[2], 0), \
  AudioConnection(m_tdm, 8, m_audio_queue[3], 0)
#endif

// Number of samples to collect to meet recording length (floor'd)
#define CONFIG_RECORDING_SAMPLE_COUNT ((size_t)( ((CONFIG_RECORDING_LENGTH / 1000) * 44100) / 256 ))
#define CONFIG_RECORDING_TOTAL_BLOCKS ((CONFIG_CHANNEL_COUNT * (CONFIG_RECORDING_SAMPLE_COUNT*256) * 2) / 512)

#endif
