#ifndef SIM800_H
#define SIM800_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Public SIM800 API ---------- */
void         sim800_restart(void);

unsigned char check_sim(void);
unsigned char check_signal_quality(void);
//unsigned char check_signal_with_restart(void);

unsigned char init_sms(void);
unsigned char init_GPRS(void);

uint8_t       bringup_gprs_and_sms(void);
//uint8_t       gprs_health_check(void);

/* ---------- Public TCP helpers (wrappers) ---------- */
//uint8_t sim_tcp_is_connected(void);
//uint8_t sim_tcp_connect_if_needed(const char* host, uint16_t port);
//void    sim_tcp_ensure_liveness(const char* host, uint16_t port, uint8_t refresh_now);
//void    sim_tcp_clear_traffic_flag(void);
//uint8_t sim_tcp_consume_traffic_flag(void);
uint8_t tcp_connect(const char* host, uint16_t port);
uint8_t tcp_keep_alive(void);
//uint8_t tcp_close();

uint8_t checking(void);
/* ---------- Externals provided elsewhere ---------- */
extern char value[16];
extern char buffer[BUFFER_SIZE];
//extern const char APP_HOST[];     /* e.g. "185.8.173.17" */
//extern const uint16_t APP_PORT;   /* e.g. 65432 */

#ifdef __cplusplus
}
#endif
#endif /* SIM800_H */
