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

#define BUF_LEN 64

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
volatile bool messageReceived = false;
volatile char currentByte = 0;
volatile int bitCount = 0;
volatile int byteIndex = 0;
uint8_t onewire_in_buf[BUF_LEN];

struct AES_ctx ctx;

uint8_t key[] = { 0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe, 0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
                      0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7, 0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4 };
uint8_t iv[16];



// prints string as hex
static void phex(uint8_t* str, uint8_t len)
{
    unsigned char i;
    for (i = 0; i < len; ++i)
        printf("%.2x", str[i]);
    printf("\n");
}

int spi_slave() {

    // Enable SPI 0 at 100 KHz and connect to GPIOs
    spi_init(SPI_PORT, 100*1000);
    spi_set_slave(SPI_PORT, true);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS,   GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    uint8_t in_buf[BUF_LEN];
    
    printf("Waiting for data on SPI\n");

    while(true) {
        spi_read_blocking(SPI_PORT, 0 ,in_buf, BUF_LEN);

        AES_CBC_decrypt_buffer(&ctx, in_buf, 64);
        
        printf("\ndecrypted data is: %s\n", in_buf);

        AES_ctx_set_iv(&ctx, iv);
        memset(in_buf, 0, 64);
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
            AES_CBC_decrypt_buffer(&ctx, in_buf, 64);
            printf("decrypted data is: %s\n\n", in_buf);

            AES_ctx_set_iv(&ctx, iv);
            memset(in_buf, 0, 64);
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
    
    printf("Waiting for data on UART\n");

    while (1) 
        tight_loop_contents();

}

static void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event) {
    static int i = 0;
    static uint8_t in_buf[BUF_LEN];

    switch (event) {
        case I2C_SLAVE_RECEIVE: // master has written some data
            in_buf[i++] = i2c_read_byte_raw(i2c);
            break;
        case I2C_SLAVE_REQUEST: // master is requesting data
            break;
        case I2C_SLAVE_FINISH: // master has signalled Stop / Restart
            AES_CBC_decrypt_buffer(&ctx, in_buf, 64);
            printf("decrypted data is: %s\n\n", in_buf);

            AES_ctx_set_iv(&ctx, iv);
            memset(in_buf, 0, 64);
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

    printf("Waiting for data on I2C\n");

}

static void onewire_handler(uint gpio, uint32_t events){
    busy_wait_us(30);
    bool bitVal = gpio_get(DATA_PIN);
    currentByte = (currentByte << 1) | bitVal; // Build up the byte from the incoming bits.
    bitCount++;
    // printf("%d\n", bitCount);
    // Once 8 bits have been received, store the byte in the buffer.
    if (bitCount % 8 == 0)
    {
        if (byteIndex < BUF_LEN)
            onewire_in_buf[byteIndex++] = currentByte;
        currentByte = 0;
    }

    // When BUF_LEN bytes have been received, flag that the message is complete.
    if (byteIndex >= BUF_LEN){
        messageReceived = true;
        gpio_set_irq_enabled_with_callback(DATA_PIN, GPIO_IRQ_EDGE_FALL, false, &onewire_handler);
    }
}

static void onewire_slave(){
    printf("Waiting for data on One Wire\n");
    gpio_init(DATA_PIN);
    gpio_set_dir(DATA_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(DATA_PIN, GPIO_IRQ_EDGE_FALL, true, &onewire_handler);
    while(true){
        if(messageReceived){
            messageReceived = false;
            AES_CBC_decrypt_buffer(&ctx, onewire_in_buf, BUF_LEN);
            printf("Received message: ");
            phex(onewire_in_buf, 32);
            printf("%s\n", onewire_in_buf);
            AES_ctx_set_iv(&ctx, iv);
            memset(onewire_in_buf, 0, BUF_LEN);
            gpio_set_irq_enabled_with_callback(DATA_PIN, GPIO_IRQ_EDGE_FALL, true, &onewire_handler);
        }
    }
}

int main()
{
    stdio_init_all();

    sleep_ms(5000);
    printf("Slave!\n");
    printf("insert IV (16 byte):");
    scanf("%16s", iv);
    printf("\nrecieved iv: %s\n", iv);

    AES_init_ctx_iv(&ctx, key, iv);
    
    if(SPI == 1){
        spi_slave();
    } else if (UART == 1){
        uart_slave();
    } else if (I2C == 1){
        i2c_slave();
    } else if (ONE == 1){
        onewire_slave();
    }

    while (1)
        tight_loop_contents();
    
}
