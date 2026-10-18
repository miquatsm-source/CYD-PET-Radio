#pragma once

#define USER_SETUP_INFO "Freenove FNK0114L 3.2 inch ST7789"
#define ST7789_DRIVER
#define TFT_WIDTH 240
#define TFT_HEIGHT 320
#define TFT_ROTATION 1

#define USE_HSPI_PORT
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1
#define TFT_BL 27
#define TFT_BACKLIGHT_ON HIGH
#define TOUCH_CS 33

#ifndef SPI_FREQUENCY
#define SPI_FREQUENCY 80000000
#endif
#define SPI_READ_FREQUENCY 20000000
#define SPI_TOUCH_FREQUENCY 2500000

#define LOAD_GLCD
#define LOAD_FONT2

#define TFT_INVERSION_ON
#define TFT_RGB_ORDER TFT_BGR
