// sim800.c  —  minimal, blocking & C89-friendly

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <glcd.h>
#include <delay.h>
#include <stddef.h>

#include "common.h"
#include "sim800.h"

// ---------------- Config ----------------
#define SIGNAL_CHECKS        5
#define MAX_SOFT_FAILS       3
//#define CIFSR_TIMEOUT_MS   6000U
//#define CIICR_TIMEOUT_MS  65000U

// ---------------- Externs you must already have elsewhere ----------------
//  - buffer/BUFFER_SIZE/UART helpers/APN/value/extract_field...
//  - APP_HOST / APP_PORT:
//      const char APP_HOST[] = "185.8.173.17";
//      const uint16_t APP_PORT = 65432;
//  - ? I? ?I?: extern const char APP_HOST[];  extern const uint16_t APP_PORT;

// ---------------- Module statics ----------------
//static uint8_t app_tcp_connected = 0;
//static uint8_t app_had_traffic_since_last_check = 0; // ??? E?C??? CI??
//static uint8_t soft_fail_count = 0;

static char at_command[60];
//static char sim_number[15];
static uint8_t attempts = 0;
static uint8_t attemptss = 0;
//static uint8_t ch; // E?C? E?E? ?C?C???C? MUX

// ---------------- Local helpers ----------------
static uint8_t is_registered_gsm_once(void)
{
    uart_buffer_reset();
    send_at_command("AT+CREG?");
    if (!read_until_keyword_keep_all(buffer, BUFFER_SIZE, 1500, "CREG")) return 0;
    if (strstr(buffer, "+CREG: 0,1") || strstr(buffer, "+CREG: 0,5")) return 1;
    return 0;
}

static uint8_t is_ipv4_string(const char *s)
{
    int dots = 0;
    if (!s) return 0;
    for (; *s; s++){
        if (*s == '.') dots++;
        else if ((*s < '0' || *s > '9') && *s != '\r' && *s != '\n') return 0;
    }
    return (dots == 3);
}

/* ============================================================
 *  EIO A) ?C??C?IC??/???C?EC?E ? CE?C??C? ?C?? (GLCD/Signal/SIM)
 * ============================================================ */

void sim800_restart(void)
{
    attempts = 0;
    glcd_outtextxy(0, 0, "Restarting SIM800 ...");

    // 1) E?E? TCP (single/mux)

    glcd_outtextxy(0, 10, "Waiting CIPCLOSE...");

    while(attempts<5)
    {
        uart_buffer_reset(); send_at_command("AT+CIPCLOSE");
        if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 1000, ""))
        {
            if (strstr(buffer, "CLOSE OK") || strstr(buffer, "NOT CONNECT") || strstr(buffer, "ERROR")) break;
        }
        //delay_ms(50);
        attempts++;
    }
    
    attempts=0;
    // 3) IC??O???I? C?E? CIP/PDP  (E?????? EI?? EC??? IC???)
    while (attempts<5) {
        uart_buffer_reset(); send_at_command("AT+CIPSHUT");
        if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 8000, "SHUT OK")) break;

//        if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 1200, "ERROR")) {
//            uart_buffer_reset(); send_at_command("AT+CIPSTATUS");
//            if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 2000, "STATE:")) {
//                if (strstr(buffer, "IP INITIAL") || strstr(buffer, "PDP DEACT")) break;
//            }
//        }
        glcd_outtextxy(0, 10, "Waiting CIPSHUT...");
        //delay_ms(50);
        attempts++;
    }
    
    attempts=0;
    // 4) Detach I?EUC (E?????? EC detach ??E? O?I)
    while (attempts<5) {
        uart_buffer_reset(); send_at_command("AT+CGATT=0");
        if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 4000, "OK")) break;

//        if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 1000, "ERROR")) {
//            uart_buffer_reset(); send_at_command("AT+CGATT?");
//            if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 1500, "CGATT")) {
//                if (strstr(buffer, "+CGATT: 0")) break;
//            }
//        }
        glcd_outtextxy(0, 10, "Waiting CGATT=0...");
        //delay_ms(50);
        attempts++;
    }

    attempts=0;
    while (attempts<5) {
        uart_buffer_reset(); send_at_command("AT+CFUN=1,1");
        read_until_keyword_keep_all(buffer, BUFFER_SIZE, 2000, "OK"); // (E??I?? C?? ??C?I)
        glcd_outtextxy(0, 10, "Rebooting, AT ...");
        while (attemptss<5) {
            uart_buffer_reset();
            send_at_command("AT");
            if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 2000, "OK")) {
                goto after_reboot;
            }
            // C?? I?C?E? ?? E?C?? C???C ??C? ????E ? delay ???C???E? E?C??
            //delay_ms(50);
            attemptss++;
        }
        attempts++;
    }



after_reboot:
    glcd_clear();
    attempts=0;
    // 6) Echo/Errors/URC SMS IC??O (E?C? E???? ?C?)
    while (attempts<5) {
        uart_buffer_reset(); send_at_command("ATE0");
        if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 2000, "OK")) break;
        delay_ms(10);
        attempts++;
    }   
    
//    while (1) {
//        uart_buffer_reset(); send_at_command("AT+CMEE=2");
//        if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 2000, "OK")) break;
//        delay_ms(10);
//    }

//    while (1) {
//        uart_buffer_reset(); send_at_command("AT+CNMI=0,0,0,0,0");
//        if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 2000, "OK")) break;
//        delay_ms(10);
//    }
    attempts=0;
    // 7) Final AT
    while (attempts<5) {
        uart_buffer_reset(); send_at_command("AT");
        if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 2000, "OK")) break;
        delay_ms(10);
        attempts++;
    }

    glcd_outtextxy(0, 30, "Restart Done!");
    //glcd_clear();
    //attempts++;
}


unsigned char check_sim(void) {
    //int stat;
    //uint8_t i = 0;
    //char *comma; 
    attempts=0;


    while(attempts<5){
        uart_buffer_reset(); send_at_command("AT+CPIN?");
        if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 5000, "CPIN")) {
            if (strstr(buffer, "READY")) break;    
        }
        attempts++;
    }
    
    attempts=0;
    while(attempts<5){
        uart_buffer_reset(); send_at_command("AT+CREG?");
        if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 5000, "CREG")) {
            if (extract_field_after_keyword(buffer, "+CREG:", 1, value, sizeof(value))) {
                if (atoi(value) == 1) break;
            }   
        }
        attempts++;
    }  
    glcd_clear();
    glcd_outtextxy(0, 20, "Network OK!");
    //delay_ms(50);
    
    return 1;
}
    

// --- ????C?/?????C?E ---
unsigned char check_signal_quality(void)
{
    int csq;
    uint8_t i;
    for (i = 0; i < SIGNAL_CHECKS; i++) { 
        uart_buffer_reset(); send_at_command("AT+CSQ");
        if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 5000, "CSQ")) {
            if (extract_field_after_keyword(buffer, "+CSQ:", 0, value, sizeof(value))) {
                csq = atoi(value);
                if (csq == 99) return 0;
                if (csq < 5) return 0;
                return 1;
            }
        }    
    }
    return 0;
}


/* =====================================
 *  EIO B) GPRS / PDP Bring-up (IP)
 * ===================================== */

unsigned char init_GPRS(void)
{
    attempts = 0;

    glcd_clear();
    glcd_outtextxy(0, 0, "Setting GPRS Mode...");
    //delay_ms(300);

    //uart_buffer_reset(); send_at_command("ATE0");          (void)read_until_keyword_keep_all(buffer, BUFFER_SIZE, 10000, "OK");
    //uart_buffer_reset(); send_at_command("AT+CMEE=2");     (void)read_until_keyword_keep_all(buffer, BUFFER_SIZE, 10000, "OK");/
//    uart_buffer_reset(); send_at_command("AT+CIPMUX=0");   (void)read_until_keyword_keep_all(buffer, BUFFER_SIZE, 10000, "OK");
//    uart_buffer_reset(); send_at_command("AT+CIPMODE=0");  (void)read_until_keyword_keep_all(buffer, BUFFER_SIZE, 10000, "OK");
//    uart_buffer_reset(); send_at_command("AT+CIPRXGET=0"); (void)read_until_keyword_keep_all(buffer, BUFFER_SIZE, 10000, "OK");
//    

    uart_buffer_reset(); send_at_command("AT+CGATT=1");    (void)read_until_keyword_keep_all(buffer, BUFFER_SIZE, 10000, "OK");      
    
    // APN
    uart_buffer_reset();
    snprintf(at_command, sizeof(at_command), "AT+CSTT=\"%s\",\"\",\"\"", APN);
    send_at_command(at_command);
    if (!read_until_keyword_keep_all(buffer, BUFFER_SIZE, 5000, "OK")){
        glcd_outtextxy(0, 30, "CSTT fail");
        return 0;
    }

    // Bring PDP
    uart_buffer_reset(); send_at_command("AT+CIICR");
    if (!read_until_keyword_keep_all(buffer, BUFFER_SIZE, 10000, "OK")){
        glcd_outtextxy(0, 30, "CIICR fail");
        return 0;
    }

    glcd_outtextxy(0, 10, "Fetching IP...");

//    while (attempts < 3) {
//        uart_buffer_reset();
//        send_at_command("AT+CIFSR");
//        read_until_keyword_keep_all(buffer, BUFFER_SIZE, 7000, "");
//
//        if (strstr(buffer, "ERROR") || strstr(buffer, "0.0.0.0")) {
//            attempts++;
//            delay_ms(50);
//            continue;
//        }
//
//        glcd_outtextxy(0, 20, buffer); // ??C?O IP
//        delay_ms(100);
//        return 1;
//    }


    while (attempts < 3) {
        uart_buffer_reset();
        send_at_command("AT+CIFSR");
        read_until_keyword_keep_all(buffer, BUFFER_SIZE, 7000, "");


        if (!(strstr(buffer, "ERROR") || strstr(buffer, "0.0.0.0"))) {

            //glcd_clear();
            glcd_outtextxy(0, 20, buffer);
            delay_ms(100);
            return 1;
        }


        //glcd_outtextxy(0, 20, buffer); // ??C?O IP
        attempts++;
        delay_ms(5);
        //return 1;
    }  
    


    glcd_outtextxy(0, 0, "No IP");
    //delay_ms(50);
    return 0;
}


uint8_t tcp_connect(const char* host, uint16_t port) {
    attempts = 0;
    glcd_clear();
    glcd_outtextxy(0, 0, "Setting TCP Mode...");
    
    
    while(attempts<5)
    {
        uart_buffer_reset(); send_at_command("AT+CIPCLOSE");
        if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 1000, ""))
        {
            if (strstr(buffer, "CLOSE OK") || strstr(buffer, "NOT CONNECT") || strstr(buffer, "ERROR")) break;
        }
        delay_ms(10);
        attempts++;
    }       
    
    uart_buffer_reset(); send_at_command("AT+CIPMUX=0");  (void)read_until_keyword_keep_all(buffer, BUFFER_SIZE, 2000, "OK");
    uart_buffer_reset(); send_at_command("AT+CIPMODE=0"); (void)read_until_keyword_keep_all(buffer, BUFFER_SIZE, 2000, "OK");
    uart_buffer_reset(); send_at_command("AT+CIPRXGET=0"); (void)read_until_keyword_keep_all(buffer, BUFFER_SIZE, 2000, "OK");
    //glcd_outtextxy(0, 10, "test 1");
    snprintf(at_command, sizeof(at_command), "AT+CIPSTART=\"TCP\",\"%s\",\"%u\"", host, (unsigned)port);
    uart_buffer_reset(); send_at_command(at_command);
    if (!read_until_keyword_keep_all(buffer, BUFFER_SIZE, 10000, "OK")){
        return 0;
    }
    
    if (strstr(buffer, "CONNECT OK") || strstr(buffer, "ALREADY CONNECT")){
        //glcd_outtextxy(0, 10, "test 2");
        return 1;
    }   
    
    if (strstr(buffer, "CONNECT FAIL") || strstr(buffer, "ERROR")){
        return 0;
    }       

    return 0;
}

/* ===========================
 *  EIO C) SMS Initialization
 * =========================== */

unsigned char init_sms(void)
{
    glcd_clear();
    glcd_outtextxy(0, 0, "Setting SMS Mode...");

    //send_at_command("AT+CFUN=1");              (void)read_until_keyword_keep_all(buffer, BUFFER_SIZE, 100, "OK");
    send_at_command("AT+CSCLK=0");             (void)read_until_keyword_keep_all(buffer, BUFFER_SIZE, 100, "OK");
    send_at_command("AT+CMGF=1");              (void)read_until_keyword_keep_all(buffer, BUFFER_SIZE, 100, "OK");

    uart_buffer_reset();
    send_at_command("AT+CNMI=2,2,0,0,0");      (void)read_until_keyword_keep_all(buffer, BUFFER_SIZE, 100, "OK");

    send_at_command("AT+CMGDA=\"DEL ALL\"");   (void)read_until_keyword_keep_all(buffer, BUFFER_SIZE, 100, "OK");

    glcd_outtextxy(0, 10, "SMS Ready.");
    delay_ms(10);
    return 1;
}


// Soft bring-up (E?C? Health Check)


/* ==========================================
 *  EIO D) C????E??E?? Bring-up E?C??O??I?
 *  (GSM + PDP + TCP + SMS) — ??? ???EC? I? O???
 * ========================================== */



/* /////////////////////////////////////////////////////////////////////////
 *  EIO E) HEALTH-CHECK ? EC?C ??? ICOE? TCP  (???E??? E?IC? ??EC?O ???)
 *  - gprs_health_check: ?C?? E?I? EEE OE?? ? ICOE? IP
 *  - ensure_app_tcp_connected / app_tcp_refresh_session / ensure_liveness
 *  - ?????C? E?C??? TCP
 * ///////////////////////////////////////////////////////////////////////// */


uint8_t bringup_gprs_and_sms(void)
{
    while (1) {
        
        check_sim();
        // OE??
        if (!check_signal_quality()) { sim800_restart(); continue; }

        // PDP/IP
        //if (!soft_bringup_ip()) { sim800_restart(); continue; }
        if (!init_GPRS()) { sim800_restart(); continue; }

        // TCP
        if (!tcp_connect(APP_HOST, APP_PORT)) { sim800_restart(); continue; }

        // SMS ON
        init_sms();

        // CNMI E??? init_sms ??C? OI? — A?CI??C??
        return 1;
    }
}




uint8_t checking(void){
    // 0) AT ???? 
    glcd_clear();
    glcd_outtextxy(0, 0, "Updating . . .");
    uart_buffer_reset(); send_at_command("AT");
    if (!read_until_keyword_keep_all(buffer, BUFFER_SIZE, 1000, "OK")) {
        glcd_outtextxy(0, 0, "No AT -> Reboot");
        sim800_restart();
        bringup_gprs_and_sms();
        return 0;                                         
    }
//    glcd_clear();
//    glcd_outtextxy(0, 0, "test 2");
    // 1) ????? attach
    uart_buffer_reset(); send_at_command("AT+CGATT?");
    if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 1000, "CGATT")) {
        if (extract_field_after_keyword(buffer, "+CGATT:", 0, value, sizeof(value))) {
            if (atoi(value) != 1) {
                glcd_outtextxy(0, 0, "CGATT=0 -> Recover");
                bringup_gprs_and_sms();
                return 0;
            }
        }
    } else {
        glcd_outtextxy(0, 0, "CGATT? timeout");
        bringup_gprs_and_sms();
        return 0;
    }
    
//    glcd_clear();
//    glcd_outtextxy(0, 0, "test 3");
    // 2) æÖÚíÊ ÔÊå
    uart_buffer_reset(); send_at_command("AT+CIPSTATUS");
    
    if (!read_until_keyword_keep_all(buffer, BUFFER_SIZE, 2000, "STATE")) {
        init_GPRS();
        tcp_connect(APP_HOST, APP_PORT); 
        init_sms();
        return 0; 
    }  
         
    if (strstr(buffer, "CONNECT OK")) { 
        //init_sms();
        return 1;  
    }
    if (strstr(buffer, "TCP CLOSED") || strstr(buffer, "IP STATUS")){
        tcp_connect(APP_HOST, APP_PORT);
        init_sms();
        return 0;
    } 
    
    if (strstr(buffer, "IP GPRSACT") || strstr(buffer, "IP INITIAL")
        || strstr(buffer, "IP START") || strstr(buffer, "PDP DEACT")){
        init_GPRS();
        tcp_connect(APP_HOST, APP_PORT);
        init_sms();
        return 0;
    }
    
    init_GPRS();
    tcp_connect(APP_HOST, APP_PORT);
    init_sms();
    return 0;
}



//uint8_t tcp_close(){
//    while(1)
//    {
//        uart_buffer_reset(); send_at_command("AT+CIPCLOSE");
//        if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 1000, ""))
//        {
//            if (strstr(buffer, "CLOSE OK") || strstr(buffer, "NOT CONNECT") || strstr(buffer, "ERROR")) break;
//        }
//        delay_ms(10);
//
//    }
//}


uint8_t tcp_keep_alive(void)
{
    char cmd[30];
//    const char* ping = "ping";

    snprintf(cmd, sizeof(cmd), "ping,%d", device_id);
//    glcd_clear();
//    glcd_outtextxy(0, 0, cmd);
//    delay_ms(500);

    // --- E?U?? URL ---
    uart_buffer_reset(); send_at_command("AT+CIPSEND");
    if(!read_until_keyword_keep_all(buffer, BUFFER_SIZE, 3000, ">"))
    {
        return 0;
    }
    uart_buffer_reset();
    send_at_command(cmd);
    putchar(0x1A);
    
//    glcd_clear();
//    glcd_outtextxy(0, 0, "test 1");
    
//    if(!read_until_keyword_keep_all(buffer, BUFFER_SIZE, 3000, "SEND OK"))
//    {
//        return 0;
//    }

//    glcd_outtextxy(0, 0, "test 2");
//    read_until_keyword_keep_all(buffer, BUFFER_SIZE, 3000, ""); 


    if (read_until_keyword_keep_all(buffer, BUFFER_SIZE, 3000, "pong"))
    {   
//        glcd_outtextxy(0, 10, buffer);
//        delay_ms(250);
        return 1;
    }

    return 1;
}

