#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"

#define NEOPIXEL_SPI_PORT spi0
#define NEOPIXEL_PIN_MOSI 7

/*
Encoded bits can have duration of 1us.
With that in mind:
- Encoded high bit can have high voltage duration of 0,75us
- Encoded low bit can have high voltade duration of 0,25us.
Thus, we can use 8Mb/s baud rate and encoded bytes 0xFC and 0xC0 for signal modulation.
*/

#define SPI_BAUDRATE 80000000 // 8MHz SPI clock. Sends 8 bits in 1us.

#define LED_COUNT 25 // Number of LEDs in strip.
#define BYTES_PER_LED 3 // Number of bytes sent per LED.
#define LED_BYTES (LED_COUNT * BYTES_PER_LED) // Total bytes sent for LED control.
#define RESET_BYTES 50 // Bytes sent for reset code. 1us/byte * 50 bytes = 50us.

#define SPI_BITS_PER_CYCLE 8 // Number of bits sent per SPI cycle.
#define SPI_BYTES (RESET_BYTES + SPI_BITS_PER_CYCLE * LED_BYTES) // Total number of bytes sent per update.

#define T1_BYTE 0xFC
#define T0_BYTE 0xC0

typedef struct {
  uint8_t G, R, B;
} pixelGRB_t;
typedef pixelGRB_t npPixel_t;

void npInit();
void npSetColor(const uint index, const uint8_t r, const uint8_t g, const uint8_t b);
void npClear();
bool npIsWritable();
void npWrite();

static uint __setByteInBuffer(uint8_t *buf, uint8_t offset, uint8_t value);

static npPixel_t __np_buffer[LED_COUNT];
static uint8_t spi_buffer[SPI_BYTES];

int main() {
  stdio_init_all();

  uint8_t red = 0;
  bool increasing = true;

  npInit();

  while (true) {
    if (red == 255)
      increasing = false;
    if (red == 0)
      increasing = true;

    if (increasing)
      red++;
    else
      red--;

    npSetColor(0, red, 0, 0);

    while (!npIsWritable());
    npWrite();
    
    sleep_ms(20);
  }
}

void npInit() {
  
  spi_init(NEOPIXEL_SPI_PORT, SPI_BAUDRATE);

  spi_set_format(
    NEOPIXEL_SPI_PORT,
    SPI_BITS_PER_CYCLE,
    1,
    1,
    SPI_MSB_FIRST
  );

  gpio_set_function(NEOPIXEL_PIN_MOSI, GPIO_FUNC_SPI);

  npClear();
}

void npSetColor(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
  __np_buffer[index].G = g;
  __np_buffer[index].R = r;
  __np_buffer[index].B = b;
}

void npClear() {
  for (uint i = 0; i < LED_COUNT; ++i) {
    __np_buffer[i].G = 0;
    __np_buffer[i].R = 0;
    __np_buffer[i].B = 0;
  }
}

bool npIsWritable() {
  return spi_is_writable(NEOPIXEL_SPI_PORT);
}

void npWrite() {
  memset(spi_buffer, 0, SPI_BYTES);
  uint offset = 0;

  for(uint i = 0; i < LED_COUNT; ++i) {
    offset = i * BYTES_PER_LED * SPI_BITS_PER_CYCLE;

    offset = __setByteInBuffer(spi_buffer, offset, __np_buffer[i].G);
    offset = __setByteInBuffer(spi_buffer, offset, __np_buffer[i].R);
    offset = __setByteInBuffer(spi_buffer, offset, __np_buffer[i].B);
  }

  spi_write_blocking(NEOPIXEL_SPI_PORT, spi_buffer, SPI_BYTES);
}

static uint __setByteInBuffer(uint8_t *buf, uint8_t offset, uint8_t value) {
  for (uint8_t i = 0; i < SPI_BITS_PER_CYCLE; ++i) {
    buf[offset++] = (value & 0x80) ? T1_BYTE : T0_BYTE;
    value <<= 1;
  }
  return offset;
}