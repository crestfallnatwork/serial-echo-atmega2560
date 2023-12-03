#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

struct serial {
  static void init(uint32_t baudrate) {
    const uint16_t ubrr = (F_CPU / (16 * baudrate)) - 1;
    UBRR0H = static_cast<uint8_t>(ubrr >> 8);
    UBRR0L = static_cast<uint8_t>(ubrr);
    UCSR0B |= (1 << TXEN0) | (1 << RXEN0);
    UCSR0C |= (1 << UCSZ00) | (1 << UCSZ01);
  }

  static bool check_recv_empty() { return !(UCSR0A & (1 << RXC0)); }
  static bool check_send_filled() { return !(UCSR0A & (1 << UDRE0)); }

  static void write(uint8_t ch) {
    while (check_send_filled())
      ;
    UDR0 = ch;
  }

  static void write(char const *str) {
    char ch = *str++;
    while (ch != '\0') {
      write(ch);
      ch = *str++;
    }
  }

  static uint8_t read() {
    while (check_recv_empty())
      ;
    return UDR0;
  }

  static uint8_t read_until(char eol, char *buffer, uint16_t size) {
    int i = 0;
    for (i = 0; i < size; ++i) {
      char ch = read();
      if (ch == eol)
        break;
      buffer[i] = ch;
    }
    return i;
  }

};

struct eeprom {
  static char read(uint16_t addr) {
    while (EECR & (1 << EEPE))
      ;
    EEARH = static_cast<uint8_t>(addr >> 8);
    EEARL = static_cast<uint8_t>(addr);
    EECR = (1 << EERE);
    return EEDR;
  }
  static void erase(uint16_t addr) {
    while (EECR & (1 << EEPE))
      ;
    EEARH = static_cast<uint8_t>(addr >> 8);
    EEARL = static_cast<uint8_t>(addr);
    EECR = (1 << EEMPE) | (1 << EEPM0);
    EECR |= (1 << EEPE);
  }
  static void write(uint16_t addr, char data) {
    while (EECR & (1 << EEPE))
      ;
    EEARH = static_cast<uint8_t>(addr >> 8);
    EEARL = static_cast<uint8_t>(addr);
    EEDR = data;
    EECR = (1 << EEMPE) | (1 << EEPM1);
    EECR |= (1 << EEPE);
  }
  static void erase_write(uint16_t addr, char data) {
    while (EECR & (1 << EEPE))
      ;
    EEARH = static_cast<uint8_t>(addr >> 8);
    EEARL = static_cast<uint8_t>(addr);
    EEDR = data;
    EECR = (1 << EEMPE);
    EECR |= (1 << EEPE);
  }
};

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

int main() {
  serial::init(2400);
  char buffer[1200];
  while (1) {
    // erase eeprom in advance, an erase-write takes 3.3ms we don't have
    for (int i = 0; i < 1024 * 2; ++i) {
      eeprom::erase(i);
    }
    // handshake
    char ch = '\n';
    while (ch != 'O') {
      ch = serial::read();
    }
    serial::write('T');
    // recive message
    int size = 0;
    for (int i = 0; i < 2000; ++i) {
      char volatile ch = serial::read();
      if (ch == '\0')
        break;
      eeprom::write(i, ch);
      buffer[i] = ch;
      size = i;
    }
    ++size;
    // check message
    serial::write("Verifying checksum...\n");
    auto chk = crc8chksum(buffer, size );
    if (chk != 0) {
      serial::write("Data corruption: CRC checksum failure\n\n\n");
    } else {
      serial::write("Checksum verifyied\n\n\n");
    }

    // update checksum
    eeprom::erase_write(size, crc8chksum(buffer, size));

    // send message
    for (int i = 0; i <= size; ++i) {
      serial::write(eeprom::read(i));
    }
    serial::write('\0');
  }
}
