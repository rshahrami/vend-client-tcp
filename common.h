#ifndef COMMON_H
#define COMMON_H

#include <mega64a.h>
#include <stdio.h>
#include <stdint.h>

#define BUFFER_SIZE 256

// --- ÊæÇÈÚ UART / AT ---
void send_at_command(char *command);
//void uart_flush0(void);
void uart_buffer_reset(void);
//unsigned char read_serial_response(char* buffer, int buffer_size, int timeout_ms, const char* expected_response);  // ÍáŞå ÈíäåÇíÊ
//unsigned char read_serial_timeout_simple(char* buffer, int buffer_size, unsigned long timeout_ms);
unsigned char read_until_keyword_keep_all(char* buffer, int buffer_size, unsigned long timeout_ms, const char* keyword);
//int extract_value_after_keyword(const char* input, const char* keyword, char* out_value, int out_size);
int extract_field_after_keyword(const char* input, const char* keyword, int field_index, char* out_value, int out_size);


// --- ãÊÛíÑåÇí ÎÇÑÌí USART0 ˜å Çíä ÊæÇÈÚ ÇÓÊİÇÏå ãí˜ääÏ ---
extern unsigned char rx_counter0;
extern unsigned char rx_wr_index0;
extern unsigned char rx_rd_index0;

extern char APN[];
extern char APP_HOST[];             // ? Çíä åã ÈÏæä const
extern const uint16_t APP_PORT;
extern int device_id;


//extern uint8_t app_had_traffic_since_last_check;



extern bit rx_buffer_overflow0;
extern char getchar(void);

#endif
