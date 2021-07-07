#include <SPI.h>
#include <NativeEthernet.h>
#include "SdFat.h"
#include <Audio.h>
#include <Wire.h>
#include "Watchdog_t4.h"
#include "ftp.h"

#define recordingLength 5000
#define sleepLength 25000
WDT_T4<WDT1> wdt;
#define numnum 120
char fnetheap[numnum * 1024];
// GUItool: begin automatically generated code
AudioInputTDM tdm1;      //xy=398,248
AudioRecordQueue queue3; //xy=605,226
AudioRecordQueue queue2; //xy=608,185
AudioRecordQueue queue1; //xy=610,148
AudioRecordQueue queue4; //xy=615,274
AudioRecordQueue queue5; //xy=656,322
AudioRecordQueue queue6; //xy=677,360
AudioConnection patchCord1(tdm1, 0, queue1, 0);
AudioConnection patchCord2(tdm1, 2, queue2, 0);
AudioConnection patchCord3(tdm1, 4, queue3, 0);
AudioConnection patchCord4(tdm1, 6, queue4, 0);
AudioConnection patchCord5(tdm1, 8, queue5, 0);
AudioConnection patchCord6(tdm1, 10, queue6, 0);
AudioControlCS42448 cs42448_1; //xy=418,471
// GUItool: end automatically generated code

// Number of channels to capture
#define CHANNEL_COUNT 4

// Filename format string
#define FILENAME_FORMAT_1 "chan1_rawdata-%d.raw"
#define FILENAME_FORMAT_2 "chan2_rawdata-%d.raw"
#define FILENAME_FORMAT_3 "chan3_rawdata-%d.raw"
#define FILENAME_FORMAT_4 "chan4_rawdata-%d.raw"
#define FILENAME_FORMAT_5 "chan5_rawdata-%d.raw"
#define FILENAME_FORMAT_6 "chan6_rawdata-%d.raw"
int resetCounter = 0;
int filesizemax = 10000;
#define FILENAME_LENGTH 32
char filename1[FILENAME_LENGTH];
char filename2[FILENAME_LENGTH];
char filename3[FILENAME_LENGTH];
char filename4[FILENAME_LENGTH];
char filename5[FILENAME_LENGTH];
char filename6[FILENAME_LENGTH];

// Maximum file name length. This allows for up to 999999
// data files with the default filename format above
#define FILENAME_LENGTH 32

// Which LED to flash for debugging
#define DEBUG_LED LED_BUILTIN

// Find the ceiling of a constant integer division at compile time
#define CEILING(x, y) (((x) + (y)-1) / (y))

char outBuf[128];

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
IPAddress server(192, 168, 42, 6); // numeric IP for Google (no DNS)
//char server[] = "www.google.com";    // name address for Google (using DNS)

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 42, 10);
IPAddress myDns(192, 168, 42, 1);

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;
EthernetClient ftpdata;
FTP<EthernetClient> g_ftp;

// Variables to measure the speed
unsigned long beginMicros, endMicros;
unsigned long byteCount = 0;
bool printWebData = true; // set to false for better speed measurement

// Panic situations mean we shouldn't continue
#define PANIC()                                            \
  do                                                       \
  {                                                        \
    debug_log("panic: exceptional condition; halting..."); \
    exit(1);                                               \
  } while (1);

#define SD_FAT_TYPE 3
// SDCARD_SS_PIN is defined for the built-in SD on some boards.
#ifndef SDCARD_SS_PIN
const uint8_t SD_CS_PIN = SS;
#else  // SDCARD_SS_PIN
// Assume built-in SD is used.
const uint8_t SD_CS_PIN = SDCARD_SS_PIN;
#endif // SDCARD_SS_PIN
// Try max SPI clock for an SD. Reduce SPI_CLOCK if errors occur.
#define SPI_CLOCK SD_SCK_MHZ(50) //really slow for ESP32
// Try to select the best SD card configuration.
#if HAS_SDIO_CLASS
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK)
#else // HAS_SDIO_CLASS
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SPI_CLOCK)
#endif // HAS_SDIO_CLASS
#if SD_FAT_TYPE == 0
SdFat g_sd_controller;
FsFile channel1_file;
#elif SD_FAT_TYPE == 1
SdFat32 g_sd_controller;
File32 channel1_file;
#elif SD_FAT_TYPE == 2
SdExFat g_sd_controller;
ExFile channel1_file;
#elif SD_FAT_TYPE == 3
SdFs g_sd_controller;

FsFile channel1_file;
FsFile channel2_file;
FsFile channel3_file;
FsFile channel4_file;
FsFile channel5_file;
FsFile channel6_file;
#else // SD_FAT_TYPE
#error Invalid SD_FAT_TYPE
#endif // SD_FAT_TYPE

bool recordingFlag = false;

#define DEBUG 1

// Macros for debugger which can be enabled with the
// DEBUG macro definition.
#ifdef DEBUG
#define debug_log(...) Serial.println(__VA_ARGS__)
#define debug_write(...) Serial.print(__VA_ARGS__)
#define debug_init() Serial.begin(9600)
#define debug_delay(n) delay(n)
#else
#define debug_log(...)
#define debug_write(...)
#define debug_init()
#define debug_delay(n)
#endif

int data_file_idx = -1;
int format_result;

unsigned long timeStart = 0;
int mode = 0;
void continueRecording();
void stopRecording();
String createNewFolder(SdFs &sd_controller);

int upload_sd_file(char* filename)
{
  int code = 0;
  FsFile file;
  char buffer[512];

  Serial.print("[+] transferring file ...");

  file = g_sd_controller.open(filename, FILE_READ);
  if( ! file ) {
    Serial.print("\b\b\bfailed: unable to open: ");
    Serial.println(filename);
    return -1;
  }

  // Open the file
  code = g_ftp.open(filename, FTP_MODE_WRITE);
  if( code != 0 ) {
    Serial.print("\b\b\bfailed: FTP status code: ");
    Serial.println(code);
    file.close();
    return code;
  }

  Serial.print("\b\b\b");
  Serial.print(filename);

  // Write data from SD card file to FTP data port
  for(size_t count = file.read(buffer, 512); count != 0; count = file.read(buffer, 512)) {
    g_ftp.write(buffer, count);
  }

  // Close file
  file.close();

  // Close/complete ftp write
  code = g_ftp.close();

  Serial.print(" (status: ");
  Serial.print(code);
  Serial.println(")");

  if( code != 0 ) {
    return code;
  }

  return 0;
}

String createNewFolder(SdFs &sd_controller)
{

  String baseFolderName = "rec";
  int basenumber = 0;

  String foldername = baseFolderName + basenumber;
  // Produce the next filename string
  while (1)
  {
    foldername = baseFolderName + basenumber;

    if (sd_controller.exists(foldername)) {
      basenumber += 1;
    } else {
      sd_controller.mkdir(foldername);
      Serial.print("[+] created folder: ");
      Serial.println(foldername);
      break;
    }
  }
  if (!sd_controller.chdir(foldername))
  {
    Serial.println("panic");
  }
  return foldername;
}
void enableEthernet()
{
  Ethernet.setStackHeap(numnum * 1024);
  Ethernet.begin(mac, ip, myDns);

  if (Ethernet.hardwareStatus() == EthernetNoHardware)
  {
    Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    while (true)
    {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
  }
  if (Ethernet.linkStatus() == LinkOFF)
  {
    Serial.println("Ethernet cable is not connected.");
  }
}
void myCallback() {
  Serial.println("FEED THE DOG SOON, OR RESET!");
}

void disableEthernet()
{
  
}

void setup()
{
  WDT_timings_t config;
  config.trigger = 30; /* in seconds, 0->128 */
  config.timeout = 60; /* in seconds, 0->128 */
  config.callback = myCallback;
  wdt.begin(config);
  delay(2000);
  // Open serial communications and wait for port to open:
  AudioMemory(256);
  Serial.begin(9600);

  // Wait for SD to be present
  debug_log("initializing sd-card");
  g_sd_controller.begin((SdioConfig(FIFO_SDIO)));
  while (!g_sd_controller.begin((SdioConfig(FIFO_SDIO))))
  {
    debug_log("error: sd initialization failed; waiting for SD card...");
    delay(1000);
    // NOTE: The old code used a different argument the second time. Is there a reason?
    // cardPresent = g_sd_controller.begin(chipSelect);
  }
  // createNewFolder(g_sd_controller);
  // start the Ethernet connection:
  Serial.println("Initialize Ethernet with static:");
  // if (Ethernet.begin(mac) == 0)
  // {
  // Serial.println("Failed to configure Ethernet using DHCP");
  // Check for Ethernet hardware present

  // try to congifure using IP address instead of DHCP:
  // Ethernet.setSocketNum(2);

  enableEthernet();
  // give the Ethernet shield a second to initialize:
  delay(1000);
  Serial.print("connecting to ");
  Serial.print(server);
  Serial.println("...");

  // if you get a connection, report back via serial:

  // Enable the codec
  int csEnable = cs42448_1.enable();

  if (!csEnable)
  {
    while (1)
    {
      Serial.println("Cirrus Chip failed to init");
      delay(1000);
    }
  }

  delay(500);

  Serial.print("Here are the default settings of register 0x05: ");
  Serial.println(cs42448_1.read(0x05), BIN);
  delay(500);

  Serial.println("Set the ADC to differential mode...");
  // cs42448_1.adcSingleEndedMode();
  cs42448_1.adcDifferentialMode();
  Serial.println("Freeze the high pass filter...");
  // cs42448_1.adcHighPassFilterFreeze();
  delay(500);

  Serial.print("Here are the updated settings of register 0x05: ");
  Serial.println(cs42448_1.read(0x05), BIN);
  delay(500);

  // Set the codec output and input levels
  cs42448_1.volume(1);
  cs42448_1.inputLevel(15.85);

  // Write some data to the debugging interface
  debug_init();
  debug_log("datalogger initializing with:");
  debug_write("  Sample Freq: ");
  debug_log("44100");


}

unsigned long timeElapsed = 0;
unsigned long timeElapsedFTP = 0;
unsigned long timeStartFTP = 0;
String basefoldername = "rec";
void loop()
{
  int startFlag = 1;
  basefoldername = createNewFolder(g_sd_controller);
  format_result = snprintf(filename1, FILENAME_LENGTH, FILENAME_FORMAT_1, 1);
  format_result = snprintf(filename2, FILENAME_LENGTH, FILENAME_FORMAT_2, 2);
  format_result = snprintf(filename3, FILENAME_LENGTH, FILENAME_FORMAT_3, 3);
  format_result = snprintf(filename4, FILENAME_LENGTH, FILENAME_FORMAT_4, 4);
  format_result = snprintf(filename5, FILENAME_LENGTH, FILENAME_FORMAT_5, 5);
  format_result = snprintf(filename6, FILENAME_LENGTH, FILENAME_FORMAT_6, 6);
  // Open the data file
  int open_1 = channel1_file.open(filename1, FILE_WRITE);
  if (!open_1)
  {
    debug_log(F("open failed"));
    PANIC();
  }

  int open_2 = channel2_file.open(filename2, FILE_WRITE);
  if (!open_2)
  {
    debug_log(F("open failed"));
    PANIC();
  }

  int open_3 = channel3_file.open(filename3, FILE_WRITE);
  if (!open_3)
  {
    debug_log(F("open failed"));
    PANIC();
  }

  int open_4 = channel4_file.open(filename4, FILE_WRITE);
  if (!open_4)
  {
    debug_log(F("open failed"));
    PANIC();
  }

  int open_5 = channel5_file.open(filename5, FILE_WRITE);
  if (!open_5)
  {
    debug_log(F("open failed"));
    PANIC();
  }

  int open_6 = channel6_file.open(filename6, FILE_WRITE);
  if (!open_6)
  {
    debug_log(F("open failed"));
    PANIC();
  }

  // // Let the debugger know we are good to go
  // debug_write(F("opened: "));
  // debug_log(filename1);
  // debug_write(F("opened: "));
  // debug_log(filename2);
  // debug_write(F("opened: "));
  // debug_log(filename3);
  // debug_write(F("opened: "));
  // debug_log(filename4);
  // debug_write(F("opened: "));
  // debug_log(filename5);
  // debug_write(F("opened: "));
  // debug_log(filename6);
  mode = 1;
  queue1.begin();
  queue2.begin();
  queue3.begin();
  queue4.begin();
  queue5.begin();
  queue6.begin();

  // Turn on LED to indicate we are sampling
  digitalWrite(DEBUG_LED, HIGH);
  recordingFlag = true;
  timeStart = millis();

  while (startFlag)
  {

    if (!recordingFlag)
    {
       wdt.feed();
      stopRecording();
      timeStartFTP = millis();
      int code = 0;

      code = g_ftp.connect(server, 21);
      if( code == 0 ) {
        code = g_ftp.auth("ftpuser", "just4munk");
        if( code == 0 ) {
          // Create and enter the recording directory
          g_ftp.mkdir(basefoldername.c_str());
          g_ftp.chdir(basefoldername.c_str());

          upload_sd_file(filename1);
          upload_sd_file(filename2);
          upload_sd_file(filename3);
          upload_sd_file(filename4);
        } else {
          Serial.print("[+] ftp authentication failed: ");
          Serial.println(code);
        }

        // Shutdown FTP connection(s)
        g_ftp.disconnect();
      } else {
        // FTP connection error
        Serial.print("ftp connection error: ");
        Serial.println(code);
      }

      Serial.println("[+] switching sdfat back to root directory");
      g_sd_controller.chdir("/");

      startFlag = 0;
      timeElapsedFTP = millis();
      while (timeElapsedFTP < (sleepLength + timeStartFTP))
      {

        digitalWrite(DEBUG_LED, LOW);
        timeElapsedFTP = millis();
      }
      if (resetCounter > 10) // reset the teensy after 10 minutes of recording, will delay until watchdog kills it
      {
      delay(10000);
      delay(10000);
      delay(10000);
      delay(10000);
      delay(10000);
      delay(10000);
      delay(10000);
      delay(10000);
      delay(10000);
      delay(10000);
      delay(10000);
      delay(10000);

      }
      resetCounter++;
    }
    else
    {
      continueRecording();
    }
    timeElapsed = millis();
    if (timeElapsed > (recordingLength + timeStart))
    {
      recordingFlag = 0;
      digitalWrite(DEBUG_LED, LOW);
    }
  }
}

void continueRecording()
{

  if (mode == 1)
  {

    if (queue1.available() >= 2)
    {
      byte buffer1[512];

      memcpy(buffer1, queue1.readBuffer(), 256);
      queue1.freeBuffer();
      memcpy(buffer1 + 256, queue1.readBuffer(), 256);
      queue1.freeBuffer();
      // write all 512 bytes to the SD card
      elapsedMicros usec = 0;
      channel1_file.write(buffer1, 512);

      // Serial.print("SD write L, us=");
      // Serial.println(usec);
    }
    if (queue2.available() >= 2)
    {
      byte buffer2[512];
      memcpy(buffer2, queue2.readBuffer(), 256);
      queue2.freeBuffer();
      memcpy(buffer2 + 256, queue2.readBuffer(), 256);
      // elapsedMicros usec = 0;
      queue2.freeBuffer();
      // write all 512 bytes to the SD card
      //elapsedMicros usec = 0;
      channel2_file.write(buffer2, 512);
      // Serial.print("SD write R, us=");
      // Serial.println(usec);
    }
    if (queue3.available() >= 2)
    {
      byte buffer3[512];
      memcpy(buffer3, queue3.readBuffer(), 256);
      queue3.freeBuffer();
      memcpy(buffer3 + 256, queue3.readBuffer(), 256);
      // elapsedMicros usec = 0;
      queue3.freeBuffer();
      // write all 512 bytes to the SD card
      //elapsedMicros usec = 0;
      channel3_file.write(buffer3, 512);
      // Serial.print("SD write R, us=");
      // Serial.println(usec);
    }
    if (queue4.available() >= 2)
    {
      byte buffer4[512];
      memcpy(buffer4, queue4.readBuffer(), 256);
      queue4.freeBuffer();
      memcpy(buffer4 + 256, queue4.readBuffer(), 256);
      // elapsedMicros usec = 0;
      queue4.freeBuffer();
      // write all 512 bytes to the SD card
      //elapsedMicros usec = 0;
      channel4_file.write(buffer4, 512);
      // Serial.print("SD write R, us=");
      // Serial.println(usec);
    }
    if (queue5.available() >= 2)
    {
      byte buffer5[512];
      memcpy(buffer5, queue5.readBuffer(), 256);
      queue5.freeBuffer();
      memcpy(buffer5 + 256, queue5.readBuffer(), 256);
      // elapsedMicros usec = 0;
      queue5.freeBuffer();
      // write all 512 bytes to the SD card
      //elapsedMicros usec = 0;
      channel5_file.write(buffer5, 512);
      // Serial.print("SD write R, us=");
      // Serial.println(usec);
    }
    if (queue6.available() >= 2)
    {
      byte buffer6[512];
      memcpy(buffer6, queue6.readBuffer(), 256);
      queue6.freeBuffer();
      memcpy(buffer6 + 256, queue6.readBuffer(), 256);
      // elapsedMicros usec = 0;
      queue6.freeBuffer();
      // write all 512 bytes to the SD card
      //elapsedMicros usec = 0;
      channel6_file.write(buffer6, 512);
      // Serial.print("SD write R, us=");
      // Serial.println(usec);
    }
  }
}

void stopRecording()
{

  if (mode == 1)
  {
    
    Serial.println("stopRecording");
    queue1.end();
    queue2.end();
    queue3.end();
    queue4.end();
    queue5.end();
    queue6.end();
    while (queue1.available() > 0)
    {
      channel1_file.write((byte *)queue1.readBuffer(), 256);
      queue1.freeBuffer();
    }
    // writeOutHeader(channel1_file);
    channel1_file.close();
    while (queue2.available() > 0)
    {
      channel2_file.write((byte *)queue2.readBuffer(), 256);
      queue2.freeBuffer();
    }
    channel2_file.close();
    while (queue3.available() > 0)
    {
      channel3_file.write((byte *)queue3.readBuffer(), 256);
      queue3.freeBuffer();
    }
    channel3_file.close();
    while (queue4.available() > 0)
    {
      channel4_file.write((byte *)queue4.readBuffer(), 256);
      queue4.freeBuffer();
    }
    channel4_file.close();
    while (queue5.available() > 0)
    {
      channel5_file.write((byte *)queue5.readBuffer(), 256);
      queue5.freeBuffer();
    }
    channel5_file.close();
    while (queue6.available() > 0)
    {
      channel6_file.write((byte *)queue6.readBuffer(), 256);
      queue6.freeBuffer();
    }
    channel6_file.close();

    mode = 0;
  }
}
