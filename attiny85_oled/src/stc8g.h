#ifndef STC8G_H
#define STC8G_H

#include <stdint.h>

// ============================================================
// STC8G1K08 Special Function Registers
// ============================================================

// --- System ---
__sfr __at(0x87) PCON;
__sfr __at(0x8E) AUXR;
__sfr __at(0xA2) P_SW2;

// --- Interrupt ---
__sfr __at(0xA8) IE;
__sfr __at(0xB8) IP;

// --- Port 3 ---
__sfr __at(0xB0) P3;
__sfr __at(0xB1) P3M1;
__sfr __at(0xB2) P3M0;

__sbit __at(0xB0) P3_0;  // SCL
__sbit __at(0xB1) P3_1;  // SDA
__sbit __at(0xB2) P3_2;
__sbit __at(0xB3) P3_3;

// --- Port 5 ---
__sfr __at(0xC8) P5;
__sfr __at(0xC9) P5M1;
__sfr __at(0xCA) P5M0;

__sbit __at(0xCC) P5_4;
__sbit __at(0xCD) P5_5;

// --- Timer 0 ---
__sfr __at(0x88) TCON;
__sfr __at(0x89) TMOD;
__sfr __at(0x8A) TL0;
__sfr __at(0x8C) TH0;

__sbit __at(0x8C) TR0;   // Timer 0 run
__sbit __at(0x8D) TF0;   // Timer 0 overflow flag

// --- ADC ---
__sfr __at(0xBC) ADC_CONTR;
__sfr __at(0xBD) ADC_RES;
__sfr __at(0xBE) ADC_RESL;
__sfr __at(0xDE) ADCCFG;

#define ADC_POWER  0x80
#define ADC_START  0x40
#define ADC_FLAG   0x20

// --- Clock ---
__sfr __at(0x97) CLKDIV;

// --- Watchdog ---
__sfr __at(0xC1) WDT_CONTR;

// --- Peripheral switch ---
#define EAXFR  0x80

#endif
