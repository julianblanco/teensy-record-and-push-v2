#ifndef __FTP_H_
#define __FTP_H_

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "IPAddress.h"
#include "Arduino.h"

#define FTP_BANNER_LEN 128
#define FTP_ADDR_LEN 32
#define FTP_MODE_READ 0
#define FTP_MODE_WRITE 1

template<typename Client>
class FTP
{
public:
  FTP() : m_authed(false), m_server(), m_data() {};
  ~FTP() {
    this->disconnect();
  };

  // Connect to the given server. This closes any active server or data connections
  int connect(IPAddress host, uint16_t port)
  {
    // Ensure we are disconnected
    this->disconnect();

    if ( this->m_server.connect(host, port) <= 0 ) {
      return 0;
    }

    // Receive the FTP banner string
    this->recvline(this->m_banner, FTP_BANNER_LEN);
    // Save server address
    this->m_server_addr = host;
  }

  // Disconnect from the server
  void disconnect()
  {
    if( this->m_server.connected() ) {
      this->sendline("QUIT");
      this->m_server.stop();
    }

    if( this->m_data.connected() )
      this->m_data.stop();

    m_authed = false;
  }

  // Authenticate with the server; you must already be connected
  int auth(const char* user, const char* password)
  {
    char buffer[256];
    int code;

    if( m_authed ) {
      return 0;
    }

    // Format and send user
    snprintf(buffer, 256, "USER %s", user);
    this->sendline(buffer);

    // Check response code
    this->recvline(buffer, 256);
    code = atoi(buffer);
    if( code != 331 ) {
      return code;
    }

    // Format and send password
    snprintf(buffer, 256, "PASS %s", password);
    this->sendline(buffer);

    // Check response code
    this->recvline(buffer, 256);
    code = atoi(buffer);
    if( code != 230 ) {
      return code;
    }

    // SUCCESS! :)
    this->m_authed = true;

    return 0;
  }

  // Open the given file in the specified mode
  int open(const char* path, int mode)
  {
    char buffer[256];
    uint16_t data_port = 0;
    int code;

    // Only one open file at a time
    if( this->m_mode != 0 ) {
      return -1;
    }

    if( mode != FTP_MODE_WRITE ) {
      return -42;
    }

    // Enter passive mode
    this->sendline("PASV");
    this->recvline(buffer, 256);
    code = atoi(buffer);

    // Ensure we entered passive mode
    if( code != 227 ) {
      return code;
    }

    // Parse out port number; assume same server address
    for(char* tok = strtok(buffer, "(,"); tok != NULL; tok = strtok(NULL, "(,")) {
      data_port = (data_port << 8) | (atoi(tok) & 0xFF);
    }

    // Ensure my above "clever" parsing is accurate... :cheese:
    if( data_port < 10000 || data_port > 10100 ) {
      return -2;
    }

    // Connect to data port
    if( this->m_data.connect(this->m_server_addr, data_port) <= 0 ) {
      return -3;
    }

    snprintf(buffer, 256, "STOR %s", path);
    this->sendline(buffer);

    this->recvline(buffer, 256);
    code = atoi(buffer);

  }

  // Close the data connection
  void close();

  // Read the given number of bytes from the opened file
  size_t read(char* buffer, size_t count);

  // Write the given number of bytes to the opened file
  size_t write(const char* buffer, size_t count);

private:

  // Read a line from the server and return the length of the string
  // If the line is longer than the buffer size, truncate the line to
  // the buffer size, but receive until a new line character. The new
  // line character is not stripped.
  size_t recvline(char* buffer, size_t size)
  {
    char lastch = 0;
    size_t idx = 0;

    while( lastch != '\n' ) {
      if( this->m_server.available() ) {
        lastch = this->m_server.read();
        if( idx < (size-1) ) {
          buffer[idx] = lastch;
          idx += 1;
        }
      }
    }

    buffer[idx] = 0;

    return idx;
  }

  void sendall(char* buffer, size_t len)
  {
    size_t idx = 0;
    size_t count = 0;
    while( idx < len ) {
      count = this->m_server.write(&buffer[idx], len-idx);
      idx += count;
    }
  }

  void sendline(char* buffer)
  {
    sendall(buffer, strlen(buffer));
    sendall("\r\n", 2);
  }

  Client m_server;
  Client m_data;
  int m_mode;
  char m_banner[FTP_BANNER_LEN];
  IPAddress m_server_addr;
  bool m_authed;
};

#endif
