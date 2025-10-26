#ifndef KEYPAD_H
#define KEYPAD_H

#include <mega64a.h>
#include <delay.h>



#define KEYPAD_PORT PORTC
#define KEYPAD_DDR DDRC
#define KEYPAD_PIN PINC
#define COL1_PIN 0
#define COL2_PIN 1
#define COL3_PIN 2
#define ROW1_PIN 7
#define ROW2_PIN 5
#define ROW3_PIN 6
#define ROW4_PIN 4

char get_key(void);
void test_keypad(void);

#endif
