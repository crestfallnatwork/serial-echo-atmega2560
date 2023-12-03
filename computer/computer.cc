#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <serial/serial.h>
#include <thread>
#include <vector>
#include <sstream>

uint8_t crc8chksum(char const *buffer, uint16_t size) {
  uint8_t crc = 0;
  uint8_t const poly = 0x07;
  while (size--) {
    crc = crc ^ *buffer++;
    for (int i = 0; i < 8; i++) {
      if (crc & (1 << 7))
        crc = (crc << 1) ^ poly;
      else
        crc = (crc << 1);
    }
  }
  return crc;
}

void log(long long time) {
  std::string bytesps = std::to_string(1 / (double(time) / 1000000000));
  std::cout << bytesps;
  std::cout << std::string(bytesps.length(), '\b');
}
int transfer(std::string towrite, bool error = false) {
  using namespace serial;
  using namespace std::literals;
  using namespace std::chrono;
  Serial ser("/dev/ttyACM0", 2400, Timeout::simpleTimeout(1000000000),
             eightbits, parity_none, stopbits_one);
  std::this_thread::sleep_for(1s);
  // handshake
  std::cout << "Starting handshake..." << std::endl;
  ser.write("O");
  if (ser.read() != "T") {
    std::cerr << "Handshake failed. Exiting..." << std::endl;
    return 1;
  }
  std::cout << "Handshake completed..." << std::endl;
  ser.flush();

  uint16_t chk = crc8chksum(towrite.c_str(), towrite.size());
  towrite.push_back(chk);
  towrite.push_back('\0');

  std::this_thread::sleep_for(2ms);
  std::cout << "Sending string..." << std::endl;

  if (error) {
    towrite[7] = '0';
  }
  // communication is slow enough so just calculate time taken to transfer
  // one byte and then inverse it to get bytes/s
  high_resolution_clock clk;
  auto start = clk.now();
  auto end = clk.now();
  std::cout << "Transfer Speed in bytes per second (Up): ";
  for (auto iter : towrite) {
    std::string str;
    str.push_back(iter);
    start = clk.now();
    ser.write(str);
    ser.flush();
    end = clk.now();
    log((end - start).count());
  }
  std::cout << std::endl;
  std::cout << "Wrote string. " << std::endl;

  char ch = 'a';
  int i = 0;
  std::string down_buffer;
  std::cout << "Transfer Speed in bytes per second (Down): ";
  while (ch != '\0') {
    start = clk.now();
    ch = ser.read()[0];
    end = clk.now();
    down_buffer.push_back(ch);
    log((end - start).count());
  }
  std::cout << std::endl;
  std::cout << "MCU respnse: " << down_buffer << std::endl;
  ser.close();
  return 0;
}

int main() {
  // load data
  std::ifstream ifs("sample.txt");
  std::string towrite;
  if (ifs.good()) {
    std::ostringstream ss;
    ss << ifs.rdbuf();
    towrite = ss.str();
  }

  std::cout << "No burst error simulation" << std::endl;
  transfer(towrite);
  std::cout << "\n\nBurst error simulation" << std::endl;
  transfer(towrite, true);
}
