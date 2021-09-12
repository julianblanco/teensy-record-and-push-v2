#include "sensor.h"

DMAMEM audio_block_t Sensor::m_audio_queue_buffer[CONFIG_AUDIO_BUFFER_SIZE];

Sensor::Sensor()
  : m_audio_patch { CONFIG_AUDIO_PATCH_INIT }
{ }
Sensor::~Sensor() { }


void Sensor::run()
{

  char recording_dir[256]; // path to the recording directory
  CONFIG_SD_FILE data_file[CONFIG_CHANNEL_COUNT]; // Open SD card file object
  byte sd_buffer[512];
  unsigned long time_stopped;
  int done = 0;

  // Initialize recording directory and data files and start
  // audio sampling queues.
  this->start_sample(recording_dir, 256, data_file);

  while( 1 )
  {
    m_watchdog.feed();

    // Reset done counter
    done = 0;

    // Sample all channels and write to disk
    for(int ch = 0; ch < CONFIG_CHANNEL_COUNT; ch++) {

      // Check if sampling is complete for this channel
      if( m_samples_collected[ch] >= CONFIG_RECORDING_SAMPLE_COUNT ) {
        done += 1;
        continue;
      }

      // Check if data is available
      if( ! m_audio_queue[ch].available() ) continue;

      // Read the data and update counters
      memcpy(&m_audio_data[ch][m_audio_offset[ch]], m_audio_queue[ch].readBuffer(), 256);
      m_audio_queue[ch].freeBuffer();
      m_audio_offset[ch] += 256;
      m_samples_collected[ch] += 1;

      // Is a block ready to flush?
      if( m_audio_offset[ch] < 4096 ) continue;

      // Flush block to disk
      data_file[ch].write(&m_audio_data[ch][0], 4096);

      // Reset counter
      m_audio_offset[ch] = 0;
    }

    // Are all channels done?
    if( done == CONFIG_CHANNEL_COUNT ) {

      // Record the time we should have stopped, since upload may take a couple seconds
      // if network latency is high.
      time_stopped = millis();

      // Close files; we are done writing
      for(int ch = 0; ch < CONFIG_CHANNEL_COUNT; ++ch) {

        // Write any non-4096-aligned data
        if( m_audio_offset[ch] != 0 ) {
          data_file[ch].write(m_audio_data[ch], m_audio_offset[ch]);
        }

        // Close the file
        data_file[ch].close();
      }

      // Stop the sampling process and flush queues
      this->stop_sample(recording_dir);

      unsigned long ellapsed = millis() - time_stopped;
      this->log("[+] upload complete after %dms\n", ellapsed);

      // Sleep for the remaining hold time
      if( ellapsed < CONFIG_HOLD_LENGTH ) {
        this->log("[+] sleep for %dms\n", CONFIG_HOLD_LENGTH-ellapsed);
        delay(CONFIG_HOLD_LENGTH - ellapsed);
      } else {
        this->log("[+] foregoing sleep due to lengthy upload\n");
      }

      // Restart recording
      this->start_sample(recording_dir, 256, data_file);
    }

  }

}

int Sensor::setup()
{
  int code = 0;

  code = this->init_serial();
  if( code != 0 ) this->panic("serial initialization failed", code);

  this->log("[-] waiting five seconds for startup sequence...");
  delay(5000);

  code = this->init_sdcard();
  if( code != 0 ) this->panic("sdcard initialization failed", code);

#if ! CONFIG_DISABLE_NETWORK
  code = this->init_ethernet();
  if( code != 0 ) this->panic("ethernet initialization failed", code);
#endif

  code = this->init_audio();
  if( code != 0 ) this->panic("audio initialization failed", code);

  code = this->init_watchdog();
  if( code != 0 ) this->panic("watchdog initialization failed", code);

  return 0;
}

int Sensor::generate_new_dir(char* recording_dir, size_t length)
{
  size_t needed;
  size_t blocks_left = m_sd.freeClusterCount() * m_sd.sectorsPerCluster();

  // Check if we have enough for this recording + 2 blocks for accounting information
  while ( blocks_left < (CONFIG_RECORDING_TOTAL_BLOCKS + 2) ) {
#if CONFIG_SD_CARD_ROLLOFF
    char channel_path[256];

    for(int id = m_first_recording; ; id++){
      // Produce a new folder path
      needed = snprintf(recording_dir, length, CONFIG_RECORDING_DIRECTORY, id);

      // Ignore non-existent entries
      if( ! m_sd.exists(recording_dir) ) continue;

      // Remove channel data
      for(int ch = 0; ; ch++) {
        snprintf(channel_path, 256, CONFIG_CHANNEL_PATH, recording_dir, ch);
        if( !m_sd.exists(channel_path) ) break;
        m_sd.remove(channel_path);
      }

      // Remove the recording directory
      m_sd.rmdir(recording_dir);

      // Update first recording ID
      m_first_recording = id + 1;

      // Update first recording tracker on disk
      CONFIG_SD_FILE file = m_sd.open("/first_recording", O_WRONLY);
      char buffer[64];
      size_t len = snprintf(buffer, 64, "%ld\n", m_first_recording);
      file.write(buffer, len);
      file.close();

      break;
    }
#else
    this->panic("sd card full and rolloff disabled!", -1);
#endif
  }

  for(int id = m_next_recording; ; id++) {
    // Produce a new folder path
    needed = snprintf(recording_dir, length, CONFIG_RECORDING_DIRECTORY, id);

    // Could we fit it in our buffer?
    if( needed > length ) {
      return 1;
    }

    // Does it exist?
    if( ! m_sd.exists(recording_dir) ) {
      // Create the new directory
      m_sd.mkdir(recording_dir);
      // Increment counter
      m_next_recording = id + 1;

      // Update the next recording tracker on disk
      CONFIG_SD_FILE file = m_sd.open("/next_recording", O_WRONLY);
      char buffer[64];
      size_t len = snprintf(buffer, 64, "%ld\n", m_next_recording);
      file.write(buffer, len);
      file.close();

      return 0;
    }
  }
}

void Sensor::start_sample(char* recording_dir, size_t length, CONFIG_SD_FILE* data_file)
{
  char channel_path[256];

  // Generate a new recording directory
  if( this->generate_new_dir(recording_dir, 256) != 0 ) {
    this->panic("recording path buffer overflow (clean out sd card?)", -1);
  }

  this->log("[+] beginning recording period for: %s\n", recording_dir);

  // Visual indicator of sampling period
  digitalWrite(CONFIG_LED, HIGH);

  // Initialize separately so they happen as close to the same time as possible
  for(int ch = 0; ch < CONFIG_CHANNEL_COUNT; ch++){
    m_audio_queue[ch].begin();
  }

  // Open each channel file
  for( int ch = 0; ch < CONFIG_CHANNEL_COUNT; ch++ ) {
    // Open the channel output file
    snprintf(channel_path, 256, CONFIG_CHANNEL_PATH, recording_dir, ch);
    if( ! data_file[ch].open(channel_path, FILE_WRITE) ) {
      this->panic("failed to open channel file", -1);
    }
    m_audio_offset[ch] = 0;
    m_samples_collected[ch] = 0;
  }
}

void Sensor::stop_sample(const char* recording_dir)
{
  CONFIG_SD_FILE channel;
#if ! CONFIG_DISABLE_NETWORK
  char channel_path[256];
  char buffer[512];
  int code;
#endif

  this->log("[+] recording period complete; uploading to: %s\n", recording_dir);

  // Visual indicator of sampling period
  digitalWrite(CONFIG_LED, LOW);

  // Ensure we don't reset
  m_watchdog.feed();

  // Complete each sampling first in order to stop all sampling at approximately the same time
  for(int ch = 0; ch < CONFIG_CHANNEL_COUNT; ch++){
    m_audio_queue[ch].end();
    m_audio_queue[ch].clear();
  }

#if ! CONFIG_DISABLE_NETWORK

  // Connect to the FTP server
  code = m_ftp.connect(CONFIG_FTP_ADDRESS, CONFIG_FTP_PORT);
  if( code != 0 ) {
    this->log("[!] failed to connect to ftp server: %d\n", code);
    return;
  }

  // Authenticate to FTP server
  code = m_ftp.auth(CONFIG_FTP_USER, CONFIG_FTP_PASSWORD);
  if( code != 0 ) {
    m_ftp.disconnect();
    this->log("[!] ftp authentication failed: %d\n", code);
    return;
  }

  // Ensure the directory exists in the FTP server
  m_ftp.mkdir(recording_dir);

  // Now, upload the data
  for(int ch = 0; ch < CONFIG_CHANNEL_COUNT; ch++) {

    // Upload the file to the FTP server
    snprintf(channel_path, 256, CONFIG_CHANNEL_PATH, recording_dir, ch);


    // Open local data file
    channel = m_sd.open(channel_path, FILE_READ);
    if( !channel ) {
      this->log("[!] failed to open sample data: %s\n", channel_path);
      continue;
    }

    // Open remote FTP destination
    code = m_ftp.open(channel_path, FTP_MODE_WRITE);
    if( code != 0 ) {
      channel.close();
      this->log("[!] failed to open remote sample data: %s (%d)\n", channel_path, code);
      continue;
    }

    // Transfer data
    for( size_t count = channel.read(buffer, 512); count != 0; count = channel.read(buffer, 512) ){
      m_ftp.write(buffer, count);
    }

    channel.close();

    // Ensure upload is reported as successful
    code = m_ftp.close();
    if( code != 0 ) {
      this->log("[!] failed to upload sample data: %s (%d)\n", channel_path, code);
      continue;
    }
  }

  // FTP no longer needed
  m_ftp.disconnect();

#endif /* ! CONFIG_DISABLE_NETWORK */

}

#if ! CONFIG_DISABLE_NETWORK

int Sensor::init_ethernet()
{
  const char* const animation = "\\|/-";
  int frame = 0;
  uint8_t mac[] = CONFIG_MAC_ADDRESS;

  // Instruct internal fnet library to use our buffer as a heap
  Ethernet.setStackHeap(this->m_network_heap, CONFIG_NETWORK_HEAP_SIZE);

  // Initialize self IP and MAC addresses
  Ethernet.begin(mac, CONFIG_SELF_ADDRESS, CONFIG_DNS_ADDRESS);

  // We need a ethernet driver or this obviously won't work
  if( Ethernet.hardwareStatus() == EthernetNoHardware ) return -1;

  // Ensure we have link
  this->log("[-] waiting for ethernet link...");
  while( Ethernet.linkStatus() != LinkON ) {
    // Silly, but erases previous message, allows us to give a lil spinny-boi
    this->log("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
    this->log("[%c] waiting for ethernet link...", animation[frame]);
    frame = (frame + 1) % 4;
    delay(1000);
  }

  // Again, silly, but I like it, okay?
  this->log("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
  this->log("                                ");
  this->log("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
  this->log("[+] initialized ethernet\n");

  return 0;
}

#endif

int Sensor::init_sdcard()
{
  const char* const animation = "\\|/-";
  int frame = 0;

  // Wait for an SD card to be inserted
  this->log("[-] waiting for sd card insertion...");
  while( ! m_sd.begin(CONFIG_SD) ) {
    // Silly, but erases previous message, allows us to give a lil spinny-boi
    this->log("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
    this->log("[%c] waiting for sd card insertion...", animation[frame]);
    frame = (frame + 1) % 4;
    delay(1000);
  }

  // Again, silly, but I like it, okay?
  this->log("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
  this->log("                                    ");
  this->log("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
  this->log("[+] initialized sd card\n");

  // We track the recording file. If drop off is disabled, then this never changes.
  if( m_sd.exists("/first_recording") ) {
    char buffer[64];
    CONFIG_SD_FILE file = m_sd.open("/first_recording", O_RDONLY);
    file.read(buffer,64);
    file.close();

    m_first_recording = atoi(buffer);
  } else {
    m_first_recording = 0;
    // Touch the marker file
    CONFIG_SD_FILE file = m_sd.open("/first_recording", O_WRONLY | O_CREAT);
    file.write("0\n", 2);
    file.close();
  }

  // This is updated after every recording, and helps start up times when the SD card
  // has a lot of recordings (e.g. >1k).
  if( m_sd.exists("/next_recording") ) {
    char buffer[64];
    CONFIG_SD_FILE file = m_sd.open("/next_recording", O_RDONLY);
    file.read(buffer,64);
    file.close();

    m_next_recording = atoi(buffer);
  } else {
    m_next_recording = 0;
    CONFIG_SD_FILE file = m_sd.open("/next_recording", O_WRONLY | O_CREAT);
    file.write("0\n", 2);
    file.close();
  }

  this->log("[+] first saved recording: %ld\n", m_first_recording);
  this->log("[+] next recording slot: %ld\n", m_next_recording);

  return 0;
}

/**
 * This function is called for the warning message. It has no access to the sensor
 * object and is just used to print a warning via serial.
 */
void watchdog_callback()
{
  Serial.println("[!] watchdog warning timeout exceeded");
}

int Sensor::init_watchdog()
{
  WDT_timings_t config;

  // Configure reset and warning timeouts
  config.trigger = CONFIG_WATCHDOG_WARNING_TIMEOUT;
  config.timeout = CONFIG_WATCHDOG_RESET_TIMEOUT;
  // This function will dump a warning to serial after the warning timeout
  config.callback = watchdog_callback;

  // Start the watchdog
  m_watchdog.begin(config);

  this->log("[+] initialized watchdog\n");

  return 0;
}

int Sensor::init_serial()
{
  Serial.begin(CONFIG_SERIAL_BAUD);

  this->log("[+] initialized serial\n");

  return 0;
}

int Sensor::init_audio()
{

  // [Beef Stroganof](https://github.com/PaulStoffregen) does `AudioMemory` which is
  // idiotically a non-captialized preprocessor macro. So we decided not to be stupid.
  AudioStream::initialize_memory(
    Sensor::m_audio_queue_buffer,
    CONFIG_AUDIO_BUFFER_SIZE
  );

  if( ! m_audio_control.enable() ){
    return -1;
  }

  // Enable differential mode
  m_audio_control.adcDifferentialMode();
  m_audio_control.adcHighPassFilterEnable();

  // Set codec input and output levels
  m_audio_control.volume(1);
  m_audio_control.inputLevel(15.85);

  delay(1000);

  this->log("[+] initialized audio controller\n");

  return 0;
}

[[noreturn]] void Sensor::panic(const char* message, int code) const
{
  this->log("panic: %s: %d\n", message, code);
  exit(1);
}

void Sensor::log(const char* fmt, ...) const
{
  char buffer[256];
  va_list args;

  // Render the formatted string to buffer
  va_start(args, fmt);
  vsnprintf(buffer, 256, fmt, args);
  va_end(args);

  // Dump the string over serial
  Serial.print(buffer);
}
