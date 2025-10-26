#include <mega64a.h>
#include <glcd.h>
#include <font5x7.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // E?C? C?E?CI? C? ECE? atoi
#include <delay.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>

#include "bitmaps.h"
#include "common.h"
#include "keypad.h"
#include "sim800.h"


//typedef unsigned char uint8_t;
//typedef unsigned int  uint16_t;
//typedef signed char   int8_t;

volatile unsigned long millis_counter = 0;
//#define APN "mcinet" // APN C??CE?? I?I ?C ?C?I ???I
//const char APN[] = "mcinet";

unsigned long last_time_health = 0;
unsigned long last_time_ping = 0;


#define glcd_pixel(x, y, color) glcd_setpixel(x, y)
#define read_flash_byte(p) (*(p))

//#define BUFFER_SIZE 512
#define HTTP_TIMEOUT_MS 5000
#define KEEPALIVE_MS 10000
uint32_t next_keepalive_at = 0;


#define MOTOR_DDR DDRE
#define MOTOR_PORT PORTE
#define MOTOR_PIN_1 2
#define MOTOR_PIN_2 3
#define MOTOR_PIN_3 4


#define BUZER_PORT PORTF
#define BUZER_PIN 1


//char ip_address_buffer[16];

char value[16];
char buffer[BUFFER_SIZE];

volatile bit sms_received = 0;

char header_buffer[100];
char content_buffer[100];
//int header_index = 0;
//int content_index = 0;


#define DATA_REGISTER_EMPTY (1<<UDRE0)
#define RX_COMPLETE (1<<RXC0)
#define FRAMING_ERROR (1<<FE0)
#define PARITY_ERROR (1<<UPE0)
#define DATA_OVERRUN (1<<DOR0)

// USART0 Receiver buffer
#define RX_BUFFER_SIZE0 100
char rx_buffer0[RX_BUFFER_SIZE0];

#if RX_BUFFER_SIZE0 <= 256
unsigned char rx_wr_index0=0,rx_rd_index0=0;
#else
unsigned int rx_wr_index0=0,rx_rd_index0=0;
#endif

#if RX_BUFFER_SIZE0 < 256
unsigned char rx_counter0=0;
#else
unsigned int rx_counter0=0;
#endif

// This flag is set on USART0 Receiver buffer overflow
bit rx_buffer_overflow0;

// USART0 Receiver interrupt service routine
interrupt [USART0_RXC] void usart0_rx_isr(void)
{
    char status,data;
    status=UCSR0A;
    data=UDR0;
    if ((status & (FRAMING_ERROR | PARITY_ERROR | DATA_OVERRUN))==0)
    {
        rx_buffer0[rx_wr_index0++]=data;
        #if RX_BUFFER_SIZE0 == 256
        // special case for receiver buffer size=256
        if (++rx_counter0 == 0) rx_buffer_overflow0=1;
        #else
        if (rx_wr_index0 == RX_BUFFER_SIZE0) rx_wr_index0=0;
        if (++rx_counter0 == RX_BUFFER_SIZE0)
            {
            rx_counter0=0;
            rx_buffer_overflow0=1;
            }
        #endif
    }
}

#ifndef _DEBUG_TERMINAL_IO_
// Get a character from the USART0 Receiver buffer
#define _ALTERNATE_GETCHAR_
#pragma used+
char getchar(void)
{
    char data;
    while (rx_counter0==0);
    data=rx_buffer0[rx_rd_index0++];
    #if RX_BUFFER_SIZE0 != 256
    if (rx_rd_index0 == RX_BUFFER_SIZE0) rx_rd_index0=0;
    #endif
    #asm("cli")
    --rx_counter0;
    #asm("sei")
    return data;
}
#pragma used-
#endif

// Standard Input/Output functions
#include <stdio.h>

// Timer 0 overflow interrupt service routine
interrupt [TIM0_OVF] void timer0_ovf_isr(void)
{
    //  «Ì„— 0 œ— „œ ›⁄·Ì Â— ~32.768ms ”——Ì“ „Ìùò‰Â
    millis_counter += 33;  // ÕœÊœÌ („Ìù Ê‰Ì œﬁÌﬁù — »« „Õ«”»Â prescaler œ—” ‘ ò‰Ì)

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned long millis(void)
{
    unsigned long ms;
    #asm("cli")
    ms = millis_counter;
    #asm("sei")
    return ms;
}

int time_due(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
}

void draw_bitmap(uint8_t x, uint8_t y, __flash unsigned char* bmp, uint8_t width, uint8_t height)
{
    uint16_t byte_index;
    uint8_t page;
    uint8_t col;
    uint8_t bit_pos;
    uint8_t data;
    uint8_t pages;

    byte_index = 0;
    pages = height / 8;

    for (page = 0; page < pages; page++) {
        for (col = 0; col < width; col++) {
            data = read_flash_byte(&bmp[byte_index++]);
            for (bit_pos = 0; bit_pos < 8; bit_pos++) {
                if (data & (1 << bit_pos)) {
                    glcd_pixel(x + col, y + (page * 8) + bit_pos, 1);
                }
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

static int hexv(char ch){
    if (ch>='0'&&ch<='9') return ch-'0';
    ch = (char)toupper((unsigned char)ch);
    if (ch>='A'&&ch<='F') return 10+(ch-'A');
    return -1;
}

static int is_all_hex_and_len4(const char* s){
    size_t n=0;
    for (; s[n]; ++n){
        int h = hexv(s[n]);
        if (h < 0) return 0;  // €Ì—Âê“
    }
    return (n>0 && (n%4)==0); // ÿÊ· »«Ìœ „÷—» 4 »«‘œ
}

// --- „”Ì— UCS2-Âê“ (06F0..06F9 ° 0660..0669) ---
static void ucs2hex_digits_to_ascii_core(const char *in, char *out, size_t outsz){
    size_t w = 0;
    size_t i;
    unsigned cp;
    
    for (i = 0; in[i] && in[i+3] && w+1 < outsz; i += 4) {
        int h0=hexv(in[i]), h1=hexv(in[i+1]), h2=hexv(in[i+2]), h3=hexv(in[i+3]);
        if (h0<0||h1<0||h2<0||h3<0) { out[w++]='?'; break; }
        cp = (unsigned)((h0<<12)|(h1<<8)|(h2<<4)|h3);

        if      (cp>=0x06F0 && cp<=0x06F9) out[w++] = '0' + (char)(cp - 0x06F0); // ›«—”Ì
        else if (cp>=0x0660 && cp<=0x0669) out[w++] = '0' + (char)(cp - 0x0660); // ⁄—»Ì-Â‰œÌ
        else if (cp < 128)                 out[w++] = (char)cp;                  // ASCII
        else                               out[w++] = '?';
    }
    out[w] = '\0';
}

// --- „”Ì— ASCII/UTF-8: «—ﬁ«„ ›«—”Ì/⁄—»Ì —«  »œÌ· ò‰° »ﬁÌÂ —« ⁄»Ê— »œÂ ---
static void normalize_digits_utf8_core(const char *in, char *out, size_t outsz){
    size_t w = 0;
    size_t i;
    unsigned char c;
    
    for (i = 0; in[i] && w+1 < outsz; ){
        c = (unsigned char)in[i];

        if (c < 0x80){ // ASCII
            out[w++] = (char)c;
            i += 1;
            continue;
        }
        // U+06F0..U+06F9 = Persian digits: UTF-8 = 0xDB 0xB0..0xB9
        if (in[i] == (char)0xDB && (unsigned char)in[i+1] >= 0xB0 && (unsigned char)in[i+1] <= 0xB9){
            out[w++] = '0' + ((unsigned char)in[i+1] - 0xB0);
            i += 2;
            continue;
        }
        // U+0660..U+0669 = Arabic-Indic: UTF-8 = 0xD9 0xA0..0xA9
        if (in[i] == (char)0xD9 && (unsigned char)in[i+1] >= 0xA0 && (unsigned char)in[i+1] <= 0xA9){
            out[w++] = '0' + ((unsigned char)in[i+1] - 0xA0);
            i += 2;
            continue;
        }
        // ”«Ì— ò«—«ò —Â«Ì ç‰œ»«Ì Ì —« (”«œÂ) ⁄»Ê— »œÂ
        out[w++] = in[i++];
    }
    out[w] = '\0';
}

// --- —«»ÿ Ê«Õœ: ŒÊœ ‘ŒÌ’ ---
void normalize_digits_auto(const char *in, char *out, size_t outsz){
    if (!in || !out || outsz==0){ if(outsz) out[0]='\0'; return; }

    // Õ–› CR/LF Ê ›«’·ÂùÂ«Ì «‰ Â«ÌÌ ”«œÂ («Œ Ì«—Ì)
    // „Ìù Ê‰Ì ﬁ»· «“ ’œ« “œ‰ Â„ «Ì‰ Å«ò”«“Ì —« «‰Ã«„ »œÌ

    if (is_all_hex_and_len4(in)){
        ucs2hex_digits_to_ascii_core(in, out, outsz);
    } else {
        normalize_digits_utf8_core(in, out, outsz); // ASCII Ì« UTF-8
    }
}

///////////////////////////////////////////////////////////////////////////////////////////


void activate_motor(int product_id)
{
    unsigned char motor_pin;
    char motor_msg[20];
    int timeout = 1000;

    switch (product_id)
    {
        case 1: motor_pin = MOTOR_PIN_1; break;
        case 2: motor_pin = MOTOR_PIN_2; break;
        case 3: motor_pin = MOTOR_PIN_3; break;
        default: return;
    }

    sprintf(motor_msg, "MOTOR %d ON!", product_id);
    glcd_clear();
    glcd_outtextxy(10, 20, motor_msg);
    MOTOR_PORT |= (1 << motor_pin);

    while (!(PIND & (1 << PIND1)) && timeout > 0)
    {
        delay_ms(1);
        timeout--;
    }

    MOTOR_PORT &= ~(1 << motor_pin);
//    glcd_clear();
//    glcd_outtextxy(10, 20, motor_msg);

    glcd_clear();
//    draw_bitmap(0, 0, mahsol_ra_bardarid, 128, 64);
//    delay_ms(80);
}


///////////////////////////////////////////////////////////////////////////////
//unsigned char send_data(const char* base_url, const char* phone_number, int product_id, int device_id) {
unsigned char send_data(const char* phone_number, int device_id, int product_id) {
    // C??C? ?EU???C (C89)
    char cmd[100];

    snprintf(cmd, sizeof(cmd), "2,%s,%d,%d", phone_number, device_id, product_id);
//    glcd_clear();
//    glcd_outtextxy(0, 0, cmd);
//    delay_ms(500);
    // --- E?U?? URL ---
    uart_buffer_reset(); send_at_command("AT+CIPSEND");
    if(!read_until_keyword_keep_all(buffer, BUFFER_SIZE, 3000, ">"))
    {
//        glcd_clear();
//        glcd_outtextxy(0, 10, "test  1");
//        delay_ms(250);
        return 0;
    }
    uart_buffer_reset();
    send_at_command(cmd);
    putchar(0x1A);

    if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 3000, ""))
    {
        if (strstr(buffer, "200")){
//            glcd_outtextxy(0, 10, "test  2");
//            delay_ms(250);
            return 1;
        }
        else return 0;
    }


//    if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 3000, ""))
//    {
//        if (strstr(buffer, "200")){
////            glcd_outtextxy(0, 10, "test  2");
////            delay_ms(250);
//            return 1;
//        }
//        else return 0;
//    }

    return 0;
}
/////////////////////////////////////////////////////////////////////////////////



//unsigned char get_data(const char* base_url, const char* phone_number) {
unsigned char get_data(const char* phone_number) {

    char cmd[100];

    // ??C?O ??C? ??? GLCD
    glcd_clear();
    draw_bitmap(0, 0, lotfan_montazer_bemanid, 128, 64);
    
    //tcp_connect(APP_HOST, APP_PORT);

    snprintf(cmd, sizeof(cmd), "%d,%s", 1, phone_number);

//    glcd_clear();
//    glcd_outtextxy(0, 0, cmd);
//    delay_ms(500);
    // --- E?U?? URL ---
    uart_buffer_reset();
    send_at_command("AT+CIPSEND");
    if(!read_until_keyword_keep_all(buffer, BUFFER_SIZE, 3000, ">"))
    {
//        glcd_clear();
//        glcd_outtextxy(0, 10, "test  1");
//        delay_ms(250);
        return 0;
    }
    uart_buffer_reset();
    send_at_command(cmd);
    putchar(0x1A);

    if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 3000, ""))
    {
        if (strstr(buffer, "200")){
//            glcd_outtextxy(0, 10, "test  2");
//            delay_ms(250);
            return 1;
        }
        else return 0;
    }


//    if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 3000, ""))
//    {
//        if (strstr(buffer, "200")){
////            glcd_outtextxy(0, 10, "test  2");
////            delay_ms(250);
//            return 1;
//        }
//        else return 0;
//    }

    return 0;
}


void handle_sms(void)
{
    char normalized_sms[100];
    //char tmp[2];
    //const char* server_url_post = "http://185.8.173.17:8000/home/post/";
    int product_id = 0;
    int timeout_counter = 0;
    char key_pressed;
    char *tok, phone[32];
    tok = strtok(header_buffer, "\"");      // ?IC ??I? +CMT:
    tok = strtok(NULL, "\"");               // C???EC? O?C?? ICI? ??E?O?
    if (tok) strcpy(phone, tok);
    else    strcpy(phone, "unknown"); 
    
    // ---  »œÌ· «—ﬁ«„ ›«—”Ì UCS2 »Â «‰ê·Ì”Ì ---
    normalize_digits_auto(content_buffer, normalized_sms, sizeof(normalized_sms)); 
    
    glcd_clear();
    glcd_outtextxy(0,0,"SMS from:");
    glcd_outtextxy(0,10,phone);
    //glcd_outtextxy(0,20,content_buffer);
    glcd_outtextxy(0,20,normalized_sms);
    delay_ms(250);
    
    //content_buffer = normalized_sms;

//////////////////////////////////////////////////////////////////////////////////////////////

//    if (strcmp(content_buffer, "1") == 0 || strcmp(content_buffer, "2") == 0 || strcmp(content_buffer, "3") == 0)
    if (strcmp(normalized_sms, "1") == 0 || strcmp(normalized_sms, "2") == 0 || strcmp(normalized_sms, "3") == 0)
    {
//        product_id = 1;
//        device_id = 1;
//        send_data(server_url_post, phone, product_id, device_id);

        //if (get_data(server_url_post, phone))
        if (get_data(phone))
        {
            glcd_clear();

            draw_bitmap(0, 0, adad_ra_vared_namaeid, 128, 64);
            draw_bitmap(0, 32, square, 128, 32);
            glcd_outtextxy(64, 42, normalized_sms);
//            BUZER_PORT |= (1 << BUZER_PIN);
//            delay_ms(80);
//            BUZER_PORT &= ~(1 << BUZER_PIN);

            key_pressed = 0;
            for (timeout_counter = 0; timeout_counter < 200; timeout_counter++)
            {
                key_pressed = get_key();
                if (key_pressed != 0) break;
                delay_ms(10);
            }

            if (key_pressed == 0)
            {
                glcd_clear();
                glcd_outtextxy(10, 25, "Timeout! Try again.");
                delay_ms(200);
            }
            else
            {
//                glcd_clear();
//                tmp[0] = content_buffer[0];
//                tmp[1] = '\0';
//                glcd_outtextxy(5, 25, tmp);
//
//                // ‰„«Ì‘ key_pressed
//                tmp[0] = key_pressed;
//                tmp[1] = '\0';
//                glcd_outtextxy(5, 45, tmp);
//                delay_ms(1000);
                // Êﬁ Ì ò·ÌœÂ« 0..9 »Â ’Ê—  ⁄œœ »—„Ìùê—œ‰
//                if (key_pressed >= 0 && key_pressed <= 9) {
//                    key_pressed = '0' + key_pressed;   //  »œÌ· »Â ò«—«ò — '0'..'9'
//                }
                if (key_pressed == normalized_sms[0])
                {
                    glcd_clear();
//                    glcd_outtextxy(5, 25, "test 3!");
//                    delay_ms(300);
                    product_id = normalized_sms[0] - '0';
                    activate_motor(product_id);

                    glcd_clear();
                    draw_bitmap(0, 0, mahsol_ra_bardarid, 128, 64);
//                    BUZER_PORT |= (1 << BUZER_PIN);
//                    delay_ms(100);
//                    BUZER_PORT &= ~(1 << BUZER_PIN);

//                    send_data(server_url_post, phone, product_id, device_id);
                    send_data(phone, device_id, product_id);
                    delay_ms(150);
                }
                else
                {

                    glcd_clear();
                    //glcd_outtextxy(5, 25, "test 2!");
                    draw_bitmap(0, 0, shomare_soton_dorost_vared_nashode, 128, 32);
                    delay_ms(300);
                }
            }
        }
        else
        {
            glcd_clear();
            draw_bitmap(0, 0, tedad_bish_az_had, 128, 64);
            delay_ms(300);
        }
    }

    else
    {
        glcd_clear();
        draw_bitmap(0, 0, shomare_dorost_payamak_nashode, 128, 64);
        delay_ms(200);

    }

////////////////////////////////////////////////////////////////////////////////////////////
    sms_received = 0;  // A?CI? E?C? ??C? E?I?
}


void process_uart_data(void)
{
    
    static uint8_t phase = 0;   // 0=I?C?I? ?I?? 1=I?C?I? ??E?C
    static uint8_t h_idx = 0;
    static uint8_t c_idx = 0;
    char d;

    while (rx_counter0 > 0 && !sms_received) {
        d = getchar();

        if (phase == 0) {
            // -- I?C?I? ?I? EC '\n' --
            if (d == '\n') {
                header_buffer[h_idx] = '\0';
                // ??? C?? ?I? ?C??? SMS ECOI? E??? ??CU I?C?I? ??E?C
                if (strstr(header_buffer, "+CMT:") != NULL) {
                    phase = 1;
                    c_idx = 0;  // A?CI? E?C? ?? ??I? content_buffer
                }
                // ???O? h_idx ?C ???E ?? EC I??? E?I? C? C?? ?? O??I
                h_idx = 0;
            }
            else if (d != '\r') {
                // ?I???? ?C?C?E? I? header_buffer
                if (h_idx < sizeof(header_buffer) - 1)
                    header_buffer[h_idx++] = d;
            }
        }
        else {
            // -- I?C?I? ??E?C EC '\n' --
            if (d == '\n') {
                content_buffer[c_idx] = '\0';
                sms_received = 1;    // ?? SMS ?C?? I??C?E OI
                phase = 0;           // E?C? ??C? E?I? I?EC?? E??????I?? E? ?C? 0
                h_idx = 0;           // C??I????C ?C ?? ?C? ???????
                c_idx = 0;
            }
            else if (d != '\r') {
                if (c_idx < sizeof(content_buffer) - 1)
                    content_buffer[c_idx++] = d;
            }
        }
    }
}



void main(void)
{

    GLCDINIT_t glcd_init_data;


    DDRA=(0<<DDA7) | (0<<DDA6) | (0<<DDA5) | (0<<DDA4) | (0<<DDA3) | (0<<DDA2) | (0<<DDA1) | (0<<DDA0);
    PORTA=(0<<PORTA7) | (0<<PORTA6) | (0<<PORTA5) | (0<<PORTA4) | (0<<PORTA3) | (0<<PORTA2) | (0<<PORTA1) | (0<<PORTA0);

    DDRB=(0<<DDB7) | (0<<DDB6) | (0<<DDB5) | (0<<DDB4) | (0<<DDB3) | (0<<DDB2) | (0<<DDB1) | (0<<DDB0);
    PORTB=(1<<PORTB7) | (1<<PORTB6) | (1<<PORTB5) | (1<<PORTB4) | (0<<PORTB3) | (0<<PORTB2) | (0<<PORTB1) | (0<<PORTB0);

    DDRC=(0<<DDC7) | (0<<DDC6) | (0<<DDC5) | (0<<DDC4) | (0<<DDC3) | (1<<DDC2) | (1<<DDC1) | (1<<DDC0);
    PORTC=(1<<PORTC7) | (1<<PORTC6) | (1<<PORTC5) | (1<<PORTC4) | (0<<PORTC3) | (0<<PORTC2) | (0<<PORTC1) | (0<<PORTC0);

    DDRD=(0<<DDD7) | (0<<DDD6) | (0<<DDD5) | (0<<DDD4) | (0<<DDD3) | (0<<DDD2) | (0<<DDD1) | (0<<DDD0);
    PORTD=(0<<PORTD7) | (0<<PORTD6) | (0<<PORTD5) | (0<<PORTD4) | (0<<PORTD3) | (0<<PORTD2) | (1<<PORTD1) | (0<<PORTD0);

    DDRE=(0<<DDE7) | (0<<DDE6) | (1<<DDE5) | (1<<DDE4) | (1<<DDE3) | (1<<DDE2) | (0<<DDE1) | (0<<DDE0);
    PORTE=(0<<PORTE7) | (0<<PORTE6) | (0<<PORTE5) | (0<<PORTE4) | (0<<PORTE3) | (0<<PORTE2) | (0<<PORTE1) | (0<<PORTE0);

    DDRF=(0<<DDF7) | (0<<DDF6) | (0<<DDF5) | (0<<DDF4) | (0<<DDF3) | (0<<DDF2) | (1<<DDF1) | (0<<DDF0);
    PORTF=(0<<PORTF7) | (0<<PORTF6) | (0<<PORTF5) | (0<<PORTF4) | (0<<PORTF3) | (0<<PORTF2) | (0<<PORTF1) | (0<<PORTF0);

    DDRG=(0<<DDG4) | (0<<DDG3) | (0<<DDG2) | (0<<DDG1) | (0<<DDG0);
    PORTG=(0<<PORTG4) | (0<<PORTG3) | (0<<PORTG2) | (0<<PORTG1) | (0<<PORTG0);

    // Timer Period: 32.768 ms
    ASSR=0<<AS0;
    TCCR0=(0<<WGM00) | (0<<COM01) | (0<<COM00) | (0<<WGM01) | (1<<CS02) | (1<<CS01) | (1<<CS00);
    TCNT0=0x00;
    OCR0=0x00;

    TCCR1A=(0<<COM1A1) | (0<<COM1A0) | (0<<COM1B1) | (0<<COM1B0) | (0<<COM1C1) | (0<<COM1C0) | (0<<WGM11) | (0<<WGM10);
    TCCR1B=(0<<ICNC1) | (0<<ICES1) | (0<<WGM13) | (0<<WGM12) | (0<<CS12) | (0<<CS11) | (0<<CS10);
    TCNT1H=0x00;
    TCNT1L=0x00;
    ICR1H=0x00;
    ICR1L=0x00;
    OCR1AH=0x00;
    OCR1AL=0x00;
    OCR1BH=0x00;
    OCR1BL=0x00;
    OCR1CH=0x00;
    OCR1CL=0x00;

    TCCR2=(0<<WGM20) | (0<<COM21) | (0<<COM20) | (0<<WGM21) | (0<<CS22) | (0<<CS21) | (0<<CS20);
    TCNT2=0x00;
    OCR2=0x00;


    TCCR3A=(0<<COM3A1) | (0<<COM3A0) | (0<<COM3B1) | (0<<COM3B0) | (0<<COM3C1) | (0<<COM3C0) | (0<<WGM31) | (0<<WGM30);
    TCCR3B=(0<<ICNC3) | (0<<ICES3) | (0<<WGM33) | (0<<WGM32) | (0<<CS32) | (0<<CS31) | (0<<CS30);
    TCNT3H=0x00;
    TCNT3L=0x00;
    ICR3H=0x00;
    ICR3L=0x00;
    OCR3AH=0x00;
    OCR3AL=0x00;
    OCR3BH=0x00;
    OCR3BL=0x00;
    OCR3CH=0x00;
    OCR3CL=0x00;

    TIMSK=(0<<OCIE2) | (0<<TOIE2) | (0<<TICIE1) | (0<<OCIE1A) | (0<<OCIE1B) | (0<<TOIE1) | (0<<OCIE0) | (1<<TOIE0);
    ETIMSK=(0<<TICIE3) | (0<<OCIE3A) | (0<<OCIE3B) | (0<<TOIE3) | (0<<OCIE3C) | (0<<OCIE1C);

    EICRA=(0<<ISC31) | (0<<ISC30) | (0<<ISC21) | (0<<ISC20) | (0<<ISC11) | (0<<ISC10) | (0<<ISC01) | (0<<ISC00);
    EICRB=(0<<ISC71) | (0<<ISC70) | (0<<ISC61) | (0<<ISC60) | (0<<ISC51) | (0<<ISC50) | (0<<ISC41) | (0<<ISC40);
    EIMSK=(0<<INT7) | (0<<INT6) | (0<<INT5) | (0<<INT4) | (0<<INT3) | (0<<INT2) | (0<<INT1) | (0<<INT0);

    UCSR0A=(0<<RXC0) | (0<<TXC0) | (0<<UDRE0) | (0<<FE0) | (0<<DOR0) | (0<<UPE0) | (0<<U2X0) | (0<<MPCM0);
    UCSR0B=(1<<RXCIE0) | (0<<TXCIE0) | (0<<UDRIE0) | (1<<RXEN0) | (1<<TXEN0) | (0<<UCSZ02) | (0<<RXB80) | (0<<TXB80);
    UCSR0C=(0<<UMSEL0) | (0<<UPM01) | (0<<UPM00) | (0<<USBS0) | (1<<UCSZ01) | (1<<UCSZ00) | (0<<UCPOL0);
    UBRR0H=0x00;
    UBRR0L=0x33;

    UCSR1B=(0<<RXCIE1) | (0<<TXCIE1) | (0<<UDRIE1) | (0<<RXEN1) | (0<<TXEN1) | (0<<UCSZ12) | (0<<RXB81) | (0<<TXB81);

    ACSR=(1<<ACD) | (0<<ACBG) | (0<<ACO) | (0<<ACI) | (0<<ACIE) | (0<<ACIC) | (0<<ACIS1) | (0<<ACIS0);
    SFIOR=(0<<ACME);

    ADCSRA=(0<<ADEN) | (0<<ADSC) | (0<<ADFR) | (0<<ADIF) | (0<<ADIE) | (0<<ADPS2) | (0<<ADPS1) | (0<<ADPS0);

    SPCR=(0<<SPIE) | (0<<SPE) | (0<<DORD) | (0<<MSTR) | (0<<CPOL) | (0<<CPHA) | (0<<SPR1) | (0<<SPR0);

    TWCR=(0<<TWEA) | (0<<TWSTA) | (0<<TWSTO) | (0<<TWEN) | (0<<TWIE);

    MCUCSR = (1 << JTD);
    MCUCSR = (1 << JTD);

    glcd_init_data.font=font5x7;
    glcd_init_data.readxmem=NULL;
    glcd_init_data.writexmem=NULL;
    glcd_init(&glcd_init_data);
    glcd_setfont(font5x7);

    // Global enable interrupts
    #asm("sei")

//    BUZER_PORT |= (1 << BUZER_PIN);
//    delay_ms(100);
//    BUZER_PORT &= ~(1 << BUZER_PIN);

    glcd_clear();
    glcd_outtextxy(0, 0, "Module Init...");
    delay_ms(150);

    sim800_restart();
    bringup_gprs_and_sms();


    glcd_clear();

    while (1)
    {
        static uint8_t processing_sms = 0;
        //char cmd[100];
        uint32_t now;

        // UI + UART processing
        //glcd_clear();
        draw_bitmap(0, 0, kode_mahsol_payamak_konid, 128, 64);
        process_uart_data();

        // Handle incoming SMS (blocking)
        if (sms_received) {
            processing_sms = 1;
//            BUZER_PORT |= (1 << BUZER_PIN);
//            delay_ms(100);
//            BUZER_PORT &= ~(1 << BUZER_PIN);
            handle_sms(); 
            sms_received = 0;
            processing_sms = 0;
            uart_buffer_reset();
            glcd_clear();
            next_keepalive_at = millis() + KEEPALIVE_MS;
        }
         
        now = millis();
        
        if (time_due(now, next_keepalive_at)) {
            //glcd_clear();
            tcp_keep_alive();                     // ÅÌ‰ê
            next_keepalive_at = millis() + KEEPALIVE_MS; //  ‰ŸÌ„ »—«Ì 1 œﬁÌﬁÂ »⁄œ
            uart_buffer_reset();
            //glcd_clear();
            
        }
        
//        // Periodic health check (every ~10s)
//        if (!processing_sms && (uint32_t)(millis() - last_time_health) > 10000) {
//            tcp_keep_alive();
//            last_time_health = millis();
//            uart_buffer_reset();
//            glcd_clear();
//        } 
        // Periodic health check (every ~10s)
        if (!processing_sms && (uint32_t)(millis() - last_time_ping) > 50000) {
//            glcd_clear();
//            glcd_outtextxy(0, 0, "Updating ...");
            if(checking()){
                tcp_keep_alive();
            }

            last_time_ping = millis();
            uart_buffer_reset();
            glcd_clear();
        } 
    }

}