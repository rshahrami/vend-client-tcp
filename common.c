#include "common.h"
#include <string.h>
#include <stdint.h>
#include <delay.h>
#include <mega64a.h>
#include <glcd.h>

#define glcd_pixel(x, y, color) glcd_setpixel(x, y)
#define read_flash_byte(p) (*(p))

/*
«Ì—«‰”·   mtnirancell
Â„—«Â «Ê·      mcinet
—«Ì ·            RighTel
*/

char APN[] = "mcinet";
// ?? ??????? 2 (???????)
char APP_HOST[] = "185.8.173.17";   // ? œÌêÂ const ‰»«‘Â  « »—Â RAM
const uint16_t APP_PORT = 9224;
int device_id = 7653;

//uint8_t app_had_traffic_since_last_check = 0;


// --- C??C? I?E?? AT ---
void send_at_command(char *command)
{
    printf("%s\r\n", command);
}

// --- ?C? ??I? EC?? USART0 ---
//void uart_flush0(void)
//{
//    unsigned char dummy;
//
//    // ?C? ??I? ????E? ?IE?C??C??
//    while (UCSR0A & (1<<RXC0)) {
//        dummy = UDR0;
//    }
////
////    // ?C? ??I? EC?? ????C??C??
////    rx_wr_index0 = rx_rd_index0 = 0;
////    rx_counter0 = 0;
////    rx_buffer_overflow0 = 0;
//}


void uart_buffer_reset(void) {
    rx_wr_index0 = rx_rd_index0 = 0;
    rx_counter0 = 0;
    rx_buffer_overflow0 = 0;
}





// E?E??? ECE? E?C? C?? ?C?
unsigned char read_until_keyword_keep_all(char* buffer, int buffer_size, unsigned long timeout_ms, const char* keyword) {
    int i = 0;
    unsigned long elapsed = 0;
    int found = 0;
    int keyword_len = strlen(keyword);

    memset(buffer, 0, buffer_size);

    while (elapsed < timeout_ms && i < buffer_size - 1) {
        while (rx_counter0 > 0 && i < buffer_size - 1) {
            char c = getchar();
            buffer[i++] = c;
            buffer[i] = '\0';

            // E???? ???I keyword
            if (!found && i >= keyword_len) {
                if (strstr(buffer, keyword) != NULL) {
                    found = 1;
                    elapsed = 0;   // ???E ??I? EC??? ? C?C?? EI?? CIC?? ??CE ?? E?CI
                }
            }
        }

        if (found && elapsed > 100) {
            // ?I?I 100ms E?I C? I?I? ???I? E???? E??
            break;
        }

        delay_ms(1);
        elapsed++;
    }

    return (i > 0);
}


int extract_field_after_keyword(const char* input, const char* keyword, int field_index, char* out_value, int out_size)
{
    int current_field = 0;
    int i = 0;
    const char* p = strstr(input, keyword);

    if (!p) return 0; // ???I?C?? ??IC ?OI

    p += strlen(keyword);      // E?? E?I C? ???I?C??

    // ?I ??I? ?C?????C ? EE??C ?E? C? C???? ???I
    while (*p == ' ' || *p == '\t') p++;

    while (*p && current_field <= field_index)
    {
        if (current_field == field_index)
        {
            // ??? ??I? ??IC? ???? EC ?C?C? CR, LF ?C space
            while (*p && *p != ',' && *p != '\r' && *p != '\n' && i < out_size - 1)
            {
                out_value[i++] = *p++;
            }
            out_value[i] = '\0';
            return 1; // ????
        }

        // ??E? E? ?C?C? E?I? ? ?I ??I? ?C?????C? C?C??
        while (*p && *p != ',') p++;
        if (*p == ',') p++;  // ?I ??I? ?C?C
        while (*p == ' ' || *p == '\t') p++; // ?I ??I? ?C??? E?I C? ?C?C
        current_field++;
    }

    return 0; // ???I ???I?U? ??IC ?OI
}

