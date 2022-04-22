/*
 * Record and Push Sensor for Laphable
*/
#ifndef _SENSOR_H_
#define _SENSOR_H_

#include <SD.h>
#include <SPI.h>
#include <Audio.h>
#include <Wire.h>
#include <stdint.h>

#include "Watchdog_t4.h"
#include "config.h"

class Sensor
{
// Public interface methods
public:
  Sensor();
  ~Sensor();

  /**
   * Setup the sensor
   *
   * This method will initialize all needed subsystems including, but not
   * limited to:
   * - Ethernet
   * - FTP
   * - Watchdog timer
   * - Timers
   * - Serial
   */
  int setup();

  /**
   * Execute the main loop for the sensor.
   *
   * This will begin sampling and uploading results as needed/available.
  */
  void run();

// Private internal methods
private:

  /**
   * Upload a file from the local SD card to the FTP server
   *
   * The given filename is used as a path on both the local SD card and within
   * the remote FTP server home directory and the directory should already exist.
   *
   * @param filename The name of the a file to upload to the FTP server
   * @return Zero on success, negative on internal error and positive on FTP error
   */
  int upload_sd_file(char* filename);

  /**
   * Create a new folder within the SD card which doesn't already exist. This
   * method will first chdir back to root, then locate a non-existent folder
   * name which follows the `CONFIG_RECORDING_DIRECTORY` preprocessor directive.
   *
   * The passed path array will be filled with the name of the new directory and
   * will be null-terminated. If the new directory name cannot fit in the provided
   * string, -1 is returned.
   *
   * Even on failure, the given character array will be modified while searching
   * for a non-existent directory path.
   *
   * @param path A writable buffer of `len` length to hold the new directory path.
   * @param len The length of the path array.
   * @return 0 on success or -1 if the path cannot fit in the provided buffer.
   */
  int generate_new_dir(char* path, size_t len);

  /**
   * Start the sampling process
   *
   * This will generate a new recording directory and store it in the given buffer
   * and also initialize the given data file list with open handles to the channel
   * data files. The data file array must be the same length as CONFIG_CHANNEL_COUNT.
   *
   * @param recording_dir A buffer to hold the path to the new recording directory
   * @param length The length of the recording_dir buffer
   * @param data_file A list of CONFIG_SD_FILE objects where we can open the data files.
   */
  void start_sample(char* recording_dir, size_t length, CONFIG_SD_FILE* data_file);

  /**
   * Stop sampling process and upload all channel data to FTP server.
   *
   * @param recording_dir Path to recording directory on both SD and FTP
   * @param data_file Open handles to SD files which are being sampled to
   */
  void stop_sample(const char* recording_dir);

  /**
   * The following initialization routines are called by setup().
   *
   * Each method sets up/configures a necessary subsystem, and returns zero
   * on success. Non-zero return values triggger a panic().
   */
  int init_sdcard();
  int init_watchdog();
  int init_serial();
  int init_audio();
#if ! CONFIG_DISABLE_NETWORK
  int init_ethernet();
#endif

  /**
   * Display a panic message and error code and halt the processor.
   *
   * This method **does not return**. It is used in exceptional conditions to
   * cause the process to halt immediately and provide some debugging output to
   * serial.
   *
   * @param message A message to dump to serial
   * @param code An error code to dump to serial (only used if non-zero)
   */
  [[noreturn]] void panic(const char* message, int code) const;

  /**
   * Dump a formatted log message to the serial port.
   *
   * This method takes arguments similar to printf(). The given format string
   * cannot exceed 256 characters in length and will be truncated to this length
   * when output.
   *
   * @param fmt A printf-style format string
   * @param ... Variable arguments matching the given format string
   */
  void log(const char* fmt, ...) const;

// Private internal variables used directly by our sensor
private:
  WDT_T4<WDT1> m_watchdog;
  AudioInputTDM m_tdm;
  AudioRecordQueue m_audio_queue[CONFIG_CHANNEL_COUNT];
  AudioConnection m_audio_patch[CONFIG_CHANNEL_COUNT];
  uint8_t m_audio_data[CONFIG_CHANNEL_COUNT][4096];
  uint8_t m_samples_collected[CONFIG_CHANNEL_COUNT];//number of 128 byte samples we have read
  uint16_t m_audio_offset[CONFIG_CHANNEL_COUNT];
  AudioControlCS42448 m_audio_control;
  CONFIG_SD_CONTROLLER m_sd;
  unsigned long m_next_recording;
  unsigned long m_first_recording;
  unsigned int recordmode;
  struct send_audio_data{
    uint16_t size_of_packet_including_header;
    uint8_t magic;
    uint8_t flags;
    uint16_t samples_per_channel;
    uint8_t channels;
    uint8_t sequence_number;
    uint8_t samples[WRITE_BLOCK_SIZE];
  };
  static_assert(8==offsetof(struct send_audio_data,samples ), "struct problem");
send_audio_data audio_data_frame;
#if ! CONFIG_DISABLE_NETWORK
  FTP<EthernetClient> m_ftp;

// Private internal variables not used by the sensor directly
private:
  uint8_t m_network_heap[CONFIG_NETWORK_HEAP_SIZE];
#endif

// Static variables used for audio shenanigans
private:
  static DMAMEM audio_block_t m_audio_queue_buffer[CONFIG_AUDIO_BUFFER_SIZE];
};

#endif
