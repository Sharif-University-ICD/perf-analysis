#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/i2c_slave.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"

#define CBC 1

#include "aes.h"

#define SPI 0
#define WIFI 0
#define CAN 0
#define UART 0
#define I2C 1
#define ONE 0

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
#define BAUD_RATE 115200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE

#define UART_TX_PIN 12
#define UART_RX_PIN 13

#define BUF_LEN 64

uint8_t key[] = { 0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe, 0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
                      0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7, 0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4 };
uint8_t cipher[]  = { 0xf5, 0x8c, 0x4c, 0x04, 0xd6, 0xe5, 0xf1, 0xba, 0x77, 0x9e, 0xab, 0xfb, 0x5f, 0x7b, 0xfb, 0xd6,
                    0x9c, 0xfc, 0x4e, 0x96, 0x7e, 0xdb, 0x80, 0x8d, 0x67, 0x9f, 0x77, 0x7b, 0xc6, 0x70, 0x2c, 0x7d,
                    0x39, 0xf2, 0x33, 0x69, 0xa9, 0xd9, 0xba, 0xcf, 0xa5, 0x30, 0xe2, 0x63, 0x04, 0x23, 0x14, 0x61,
                    0xb2, 0xeb, 0x05, 0xe2, 0xc3, 0x9b, 0xe9, 0xfc, 0xda, 0x6c, 0x19, 0x07, 0x8c, 0x6a, 0x9d, 0x1b };
uint8_t iv[]  = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };
uint8_t plain[] = { 0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96, 0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
                    0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c, 0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
                    0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11, 0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
                    0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17, 0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10 };


// prints string as hex
static void phex(uint8_t* str)
{
    uint8_t len = 32;

    unsigned char i;
    for (i = 0; i < len; ++i)
        printf("%.2x", str[i]);
    printf("\n");
}

// static int test_decrypt_cbc(void){
   
// //  uint8_t buffer[64];
//     struct AES_ctx ctx;

//     AES_init_ctx_iv(&ctx, key, iv);
//     AES_CBC_decrypt_buffer(&ctx, in, 64);
//     phex(in);
//     printf("CBC decrypt: ");

//     if (0 == memcmp((char*) out, (char*) in, 64)) {
//         printf("SUCCESS!\n");
//         phex(in);
//         phex(out);
// 	return(0);
//     } else {
//         printf("FAILURE!\n");
//         // phex(in);
//         // phex(out);
// 	return(1);
//     }
// }

int spi_slave() {

    printf("SPI slave example\n");

    // Enable SPI 0 at 1 MHz and connect to GPIOs
    spi_init(SPI_PORT, 1000*1000);
    spi_set_slave(SPI_PORT, true);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS,   GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    uint8_t in_buf[BUF_LEN];
    
    for (size_t i = 0; ; ++i) {
        // Write the output buffer to MISO, and at the same time read from MOSI.
        spi_read_blocking(SPI_PORT, 0 ,in_buf, BUF_LEN);

        // Write to stdio whatever came in on the MOSI line.
        printf("SPI slave says: read page %d from the MOSI line:\n", i);
        phex(in_buf);
    }
}

void on_uart_rx() {
    static int i = 0;
    static uint8_t in_buf[BUF_LEN];

    while (uart_is_readable(UART_ID)) {
        uint8_t ch = uart_getc(UART_ID);
        in_buf[i++] = ch;
        if (i == BUF_LEN) {
            i = 0;
            printf("UART says: read page from the RX line:\n");
            phex(in_buf);
        }
    }
}

void uart_slave() {
    uart_init(UART_ID, BAUD_RATE);

    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // Set UART flow control CTS/RTS, we don't want these, so turn them off
    uart_set_hw_flow(UART_ID, false, false);

    // Set our data format
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);

    // Turn off FIFO's - we want to do this character by character
    uart_set_fifo_enabled(UART_ID, false);

    int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;

    irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
    irq_set_enabled(UART_IRQ, true);

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(UART_ID, true, false);

    while (1) 
        tight_loop_contents();

}

static void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event) {
    static int i = 0;
    static uint8_t in_buf[BUF_LEN];

    switch (event) {
        case I2C_SLAVE_RECEIVE: // master has written some data
            in_buf[i++] = i2c_read_byte_raw(i2c);
            printf("I2C slave says: read page from the SDA line:\n");
            break;
        case I2C_SLAVE_REQUEST: // master is requesting data
            break;
        case I2C_SLAVE_FINISH: // master has signalled Stop / Restart
            printf("I2C slave says: read page from the SDA line:\n");
            phex(in_buf);
            i = 0;
            break;
        default:
            break;
    }
}

static void i2c_slave() {
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);

    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SCL);

    i2c_init(I2C_PORT, I2C_BAUDRATE);
    // configure I2C0 for slave mode
    i2c_slave_init(I2C_PORT, I2C_SLAVE_ADDRESS, &i2c_slave_handler);
}

int main()
{

    stdio_init_all();

    struct AES_ctx ctx;

    AES_init_ctx_iv(&ctx, key, iv);

    printf("Hello, world!\n");
    while (true) {
        sleep_ms(1000);
        if(SPI == 1){
            spi_slave();
        } else if (UART == 1){
            uart_slave();
        } else if (I2C == 1){
            i2c_slave();
            while (true)
            {
            }
            
        }
    }
}
