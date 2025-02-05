#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"

#define CBC 1

#include "aes.h"

#define SPI 0
#define WIFI 0
#define CAN 0
#define UART 0
#define I2C 0
#define ONE 1

// SPI Defines
#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19

// I2C defines
#define I2C_PORT i2c0
#define I2C_SDA 16
#define I2C_SCL 17
#define I2C_SLAVE_ADDRESS 0x22
#define I2C_BAUDRATE 100000 // 100 kHz

// UART defines
#define UART_ID uart0
#define BAUD_RATE 100000
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE

#define UART_TX_PIN 12
#define UART_RX_PIN 13

// One Wire defines
#define DATA_PIN 15

#define BUF_LEN 64

uint8_t key[] = { 0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe, 0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
                      0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7, 0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4 };
uint8_t cipher[]  = { 0xf5, 0x8c, 0x4c, 0x04, 0xd6, 0xe5, 0xf1, 0xba, 0x77, 0x9e, 0xab, 0xfb, 0x5f, 0x7b, 0xfb, 0xd6,
                    0x9c, 0xfc, 0x4e, 0x96, 0x7e, 0xdb, 0x80, 0x8d, 0x67, 0x9f, 0x77, 0x7b, 0xc6, 0x70, 0x2c, 0x7d,
                    0x39, 0xf2, 0x33, 0x69, 0xa9, 0xd9, 0xba, 0xcf, 0xa5, 0x30, 0xe2, 0x63, 0x04, 0x23, 0x14, 0x61,
                    0xb2, 0xeb, 0x05, 0xe2, 0xc3, 0x9b, 0xe9, 0xfc, 0xda, 0x6c, 0x19, 0x07, 0x8c, 0x6a, 0x9d, 0x1b };
uint8_t iv[16];
uint8_t plain[64];


// prints string as hex
static void phex(uint8_t* str)
{
    uint8_t len = 32;

    unsigned char i;
    for (i = 0; i < len; ++i)
        printf("%.2x", str[i]);
    printf("\n");
}

void spi_master() {
    printf("SPI master example\n");

    // SPI initialisation. This example will use SPI at 1MHz.
    spi_init(SPI_PORT, 100*1000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS,   GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    

    printf("SPI master says: The buffer will be written to MOSI endlessly:\n");

    while (1) {
        uint64_t start = time_us_64();
        spi_write_blocking(SPI_PORT, plain, BUF_LEN);
        uint64_t end = time_us_64();
        printf("\ntime took to write: %lld\n", end - start);
        sleep_ms(5 * 1000);
    }
}

void uart_master() {
    printf("UART master example\n");

    uart_init(UART_ID, BAUD_RATE);

    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // Set UART flow control CTS/RTS, we don't want these, so turn them off
    uart_set_hw_flow(UART_ID, false, false);

    // Set our data format
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);

    // Turn off FIFO's - we want to do this character by character
    uart_set_fifo_enabled(UART_ID, false);

    printf("UART master says: The buffer will be written to TX endlessly:\n");

    while (1) {
        uint64_t start = time_us_64();
        uart_write_blocking(UART_ID, plain, BUF_LEN);
        uint64_t end = time_us_64();
        printf("\ntime took to write: %lld\n", end - start);
        sleep_ms(5 * 1000);
    }
}

void i2c_master() {
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);

    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SCL);

    uint real_baud = i2c_init(I2C_PORT, I2C_BAUDRATE);

    printf("Requested baud rate was %d actual rate is %d\n", I2C_BAUDRATE, real_baud);

    for (uint8_t mem_address = 0;; mem_address = (mem_address + 32) % 256) {
        uint64_t start = time_us_64();
        int count = i2c_write_blocking(I2C_PORT, I2C_SLAVE_ADDRESS, plain, BUF_LEN, true);
        if (count < 0) {
            printf("Couldn't write to slave, please check your wiring!\n");
        }else{
            printf("Wrote %d byres\n", count);
        }
        uint64_t end = time_us_64();
        printf("\ntime took to write: %lld\n", end - start);
        sleep_ms(5 * 1000);
    }   
}

void onewire_master(){
    gpio_init(DATA_PIN);
    gpio_set_dir(DATA_PIN, GPIO_OUT);
    gpio_put(DATA_PIN, true);
    uint64_t start = time_us_64();
    for (int i = 0; i < BUF_LEN; i++)
        for (int j = 7; j >= 0; j--) {
            char data = (char)plain[i];
            char bitVal = (data >> j) & 0x01;
            if (bitVal == 0) {
                gpio_put(DATA_PIN, false);
                sleep_us(60);
                gpio_put(DATA_PIN, true);
                sleep_us(10);
            } else {
                gpio_put(DATA_PIN, false);
                sleep_us(10);
                gpio_put(DATA_PIN, true);
                sleep_us(60);
            }
        }
    uint64_t end = time_us_64();
    printf("\n time took to write: %lld\n", end - start);
}

int main()
{

    stdio_init_all();

    struct AES_ctx ctx;

    sleep_ms(5000);
    printf("Master!\n");
    printf("insert IV (16 byte):");

    scanf("%16s", iv);
    printf("\nrecieved iv: %s\n", iv);

    AES_init_ctx_iv(&ctx, key, iv);

    memset(plain, 0, 64);
    
    printf("insert plain text (256 byte):");
    scanf("%256s", plain);

    printf("\nPlain text data is: ");
    phex(plain);
    AES_CBC_encrypt_buffer(&ctx, plain, 64);
    printf("Ciphertext data is: ");
    phex(plain);
    
    if(SPI == 1){
        spi_master();
    }
    else if (UART == 1){
        uart_master();
    } else if (I2C == 1){
        i2c_master();
    } else if(ONE == 1){
        onewire_master();
    }

    while (1)
        tight_loop_contents();
    
        
}
