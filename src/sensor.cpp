#include "sensor.h"

Sensor::Sensor()
  : m_audio_patch { CONFIG_AUDIO_PATCH_INIT }
{ }
Sensor::~Sensor() { }


void Sensor::run()
{

  char recording_dir[256]; // path to the recording directory
  CONFIG_SD_FILE data_file[CONFIG_CHANNEL_COUNT]; // Open SD card file object
  unsigned long recording_end = 0;

  // Initialize recording directory and data files and start
  // audio sampling queues.
  this->start_sample(recording_dir, 256, data_file);

  // turn on the LED when we are sampling
  recording_end = millis() + CONFIG_SAMPLE_LENGTH;

  while( 1 )
  {

    // Sample all channels and write to disk
    for(int ch = 0; ch < CONFIG_CHANNEL_COUNT; ch++) {
      if( m_audio_queue[ch].available() < 2 ) continue;

      for(int i = 0; i < 2; ++i) {
        data_file[ch].write(m_audio_queue[ch].readBuffer(), 256);
        m_audio_queue[ch].freeBuffer();
      }
    }

    if( millis() >= recording_end ) {
      // Stop the sampling process and flush queues
      this->stop_sample(recording_dir, data_file);

      // Sleep for our hold length
      delay(CONFIG_HOLD_LENGTH);

      // Restart recording
      this->start_sample(recording_dir, 256, data_file);

      // Adjust ending time
      recording_end = millis() + CONFIG_SAMPLE_LENGTH;
    }

  }

}

int Sensor::setup()
{
  int code = 0;

  code = this->init_watchdog();
  if( code != 0 ) this->panic("watchdog initialization failed", code);

  code = this->init_serial();
  if( code != 0 ) this->panic("serial initialization failed", code);

  code = this->init_sdcard();
  if( code != 0 ) this->panic("sdcard initialization failed", code);

  code = this->init_ethernet();
  if( code != 0 ) this->panic("ethernet initialization failed", code);

  code = this->init_audio();
  if( code != 0 ) this->panic("audio initialization failed", code);

  return 0;
}

int Sensor::generate_new_dir(char* recording_dir, size_t length)
{
  size_t needed;

  for(int id = 0; ; id++) {
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

  // Open each channel file
  for( int ch = 0; ch < CONFIG_CHANNEL_COUNT; ch++ ) {

    // Open the channel output file
    snprintf(channel_path, 256, CONFIG_CHANNEL_PATH, recording_dir, ch);
    if( ! data_file[ch].open(channel_path, FILE_WRITE) ) {
      this->panic("failed to open channel file", -1);
    }

    // Initialize the queue
    m_audio_queue[ch].begin();
  }
}

void Sensor::stop_sample(const char* recording_dir, CONFIG_SD_FILE* data_file)
{
  CONFIG_SD_FILE channel;
  char channel_path[256];
  char buffer[512];
  int code;

  this->log("[+] recording period complete; uploading to: %s\n", recording_dir);

  // Visual indicator of sampling period
  digitalWrite(CONFIG_LED, LOW);

  // Ensure we don't reset
  m_watchdog.feed();

  // Complete each sampling first in order to stop all sampling at approximately the same time
  for(int ch = 0; ch < CONFIG_CHANNEL_COUNT; ch++){
    m_audio_queue[ch].end();
  }

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

  // Now, flush buffers and upload the data
  for(int ch = 0; ch < CONFIG_CHANNEL_COUNT; ch++) {

    // Flush any remaining data
    while( m_audio_queue[ch].available() > 0 ) {
      data_file[ch].write((byte*)m_audio_queue[ch].readBuffer(), 256);
      m_audio_queue[ch].freeBuffer();
    }

    // Close the file
    data_file[ch].close();

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

}

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
  while( ! m_sd.begin(SdioConfig(FIFO_SDIO)) ) {
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
  this->log("[+] initialized sd card\n");

  return 0;
}

int Sensor::init_sdcard()
{
  const char* const animation = "\\|/-";
  int frame = 0;

  // Wait for an SD card to be inserted
  this->log("[-] waiting for sd card insertion...");
  while( ! m_sd.begin(SdioConfig(FIFO_SDIO)) ) {
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

  return 0;
}

int Sensor::init_serial()
{
  Serial.begin(CONFIG_SERIAL_BAUD);

  return 0;
}

int Sensor::init_audio()
{
  if( ! m_audio_control.enable() ){
    return -1;
  }

  // Enable differential mode
  m_audio_control.adcDifferentialMode();

  // Set codec input and output levels
  m_audio_control.volume(1);
  m_audio_control.inputLevel(15.85);

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
