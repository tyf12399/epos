/*
 * vim: filetype=c:fenc=utf-8:ts=4:et:sw=4:sts=4
 */
/*
 * http://flint.cs.yale.edu/cs422/doc/art-of-asm/pdf/CH20.PDF
 */
/* ======================================================== */
/* File: KEYBOARD.C                                         */
/*                                                          */
/* Copyright (C) 2001, Daniel W. Lewis and Prentice-Hall    */
/* with minor modifications for EPOS                        */
/*                                                          */
/* Purpose: Library routines for access to the keyboard.    */
/*                                                          */
/* ======================================================== */

#include "kernel.h"
#include "cpu.h"

#define TRUE        1
#define FALSE       0

#define PRIVATE     static
#define ENTRIES(a)  (sizeof(a)/sizeof(a[0]))

typedef struct STATE
{
    int ins;
    int rshift;
    int lshift;
    int alt;
    int ctrl;
    int caps;
    int scrl;
    int num;
} __attribute__((packed)) STATE ;

PRIVATE STATE state ; /* Initialized to all FALSE by loader */

/* -------------------------------------------------------- */
/* Scan code translation table.                             */
/* The incoming scan code from the keyboard selects a row.  */
/* The modifier status selects the column.                  */
/* The word at the intersection of the two is the scan/ASCII*/
/* code to put into the PC's type ahead buffer.             */
/* If the value fetched from the table is zero, then we do  */
/* not put the character into the type ahead buffer.        */
/* -------------------------------------------------------- */
PRIVATE uint16_t scan_ascii[][8] =
{
       /* norm,  shft,   ctrl,    alt,    num,   caps,   shcap,  shnum */
/*--*/  {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
/*ESC*/ {0x011B, 0x011B, 0x011B, 0x011B, 0x011B, 0x011B, 0x011B, 0x011B},
/*1 !*/ {0x0231, 0x0221, 0x0000, 0x7800, 0x0231, 0x0231, 0x0231, 0x0321},
/*2 @*/ {0x0332, 0x0340, 0x0300, 0x7900, 0x0332, 0x0332, 0x0332, 0x0332},
/*3 #*/ {0x0433, 0x0423, 0x0000, 0x7A00, 0x0433, 0x0433, 0x0423, 0x0423},
/*4 $*/ {0x0534, 0x0524, 0x0000, 0x7B00, 0x0534, 0x0534, 0x0524, 0x0524},
/*5 %*/ {0x0635, 0x0625, 0x0000, 0x7C00, 0x0635, 0x0635, 0x0625, 0x0625},
/*6 ^*/ {0x0736, 0x075E, 0x071E, 0x7D00, 0x0736, 0x0736, 0x075E, 0x075E},
/*7 &*/ {0x0837, 0x0826, 0x0000, 0x7E00, 0x0837, 0x0837, 0x0826, 0x0826},
/*8 **/ {0x0938, 0x092A, 0x0000, 0x7F00, 0x0938, 0x0938, 0x092A, 0x092A},
/*9 (*/ {0x0A39, 0x0A28, 0x0000, 0x8000, 0x0A39, 0x0A39, 0x0A28, 0x0A28},
/*0 )*/ {0x0B30, 0x0B29, 0x0000, 0x8100, 0x0B30, 0x0B30, 0x0B29, 0x0B29},
/*- _*/ {0x0C2D, 0x0C5F, 0x0000, 0x8200, 0x0C2D, 0x0C2D, 0x0C5F, 0x0C5F},
/*= +*/ {0x0D3D, 0x0D2B, 0x0000, 0x8300, 0x0D3D, 0x0D3D, 0x0D2B, 0x0D2B},
/*bksp*/{0x0E08, 0x0E08, 0x0E7F, 0x0000, 0x0E08, 0x0E08, 0x0E08, 0x0E08},
/*Tab*/ {0x0F09, 0x0F00, 0x0000, 0x0000, 0x0F09, 0x0F09, 0x0F00, 0x0F00},
/*Q*/   {0x1071, 0x1051, 0x1011, 0x1000, 0x1071, 0x1051, 0x1051, 0x1071},
/*W*/   {0x1177, 0x1057, 0x1017, 0x1100, 0x1077, 0x1057, 0x1057, 0x1077},
/*E*/   {0x1265, 0x1245, 0x1205, 0x1200, 0x1265, 0x1245, 0x1245, 0x1265},
/*R*/   {0x1372, 0x1352, 0x1312, 0x1300, 0x1272, 0x1252, 0x1252, 0x1272},
/*T*/   {0x1474, 0x1454, 0x1414, 0x1400, 0x1474, 0x1454, 0x1454, 0x1474},
/*Y*/   {0x1579, 0x1559, 0x1519, 0x1500, 0x1579, 0x1559, 0x1579, 0x1559},
/*U*/   {0x1675, 0x1655, 0x1615, 0x1600, 0x1675, 0x1655, 0x1675, 0x1655},
/*I*/   {0x1769, 0x1749, 0x1709, 0x1700, 0x1769, 0x1749, 0x1769, 0x1749},
/*O*/   {0x186F, 0x184F, 0x180F, 0x1800, 0x186F, 0x184F, 0x186F, 0x184F},
/*P*/   {0x1970, 0x1950, 0x1910, 0x1900, 0x1970, 0x1950, 0x1970, 0x1950},
/*[ {*/ {0x1A5B, 0x1A7B, 0x1A1B, 0x0000, 0x1A5B, 0x1A5B, 0x1A7B, 0x1A7B},
/*] }*/ {0x1B5D, 0x1B7D, 0x1B1D, 0x0000, 0x1B5D, 0x1B5D, 0x1B7D, 0x1B7D},
/*entr*/{0x1C0D, 0x1C0D, 0x1C0A, 0x0000, 0x1C0D, 0x1C0D, 0x1C0A, 0x1C0A},
/*ctrl*/{0x1D00, 0x1D00, 0x1D00, 0x1D00, 0x1D00, 0x1D00, 0x1D00, 0x1D00},
/*A*/   {0x1E61, 0x1E41, 0x1E01, 0x1E00, 0x1E61, 0x1E41, 0x1E61, 0x1E41},
/*S*/   {0x1F73, 0x1F53, 0x1F13, 0x1F00, 0x1F73, 0x1F53, 0x1F73, 0x1F53},
/*D*/   {0x2064, 0x2044, 0x2004, 0x2000, 0x2064, 0x2044, 0x2064, 0x2044},
/*F*/   {0x2166, 0x2146, 0x2106, 0x2100, 0x2166, 0x2146, 0x2166, 0x2146},
/*G*/   {0x2267, 0x2247, 0x2207, 0x2200, 0x2267, 0x2247, 0x2267, 0x2247},
/*H*/   {0x2368, 0x2348, 0x2308, 0x2300, 0x2368, 0x2348, 0x2368, 0x2348},
/*J*/   {0x246A, 0x244A, 0x240A, 0x2400, 0x246A, 0x244A, 0x246A, 0x244A},
/*K*/   {0x256B, 0x254B, 0x250B, 0x2500, 0x256B, 0x254B, 0x256B, 0x254B},
/*L*/   {0x266C, 0x264C, 0x260C, 0x2600, 0x266C, 0x264C, 0x266C, 0x264C},
/*; :*/ {0x273B, 0x273A, 0x0000, 0x0000, 0x273B, 0x273B, 0x273A, 0x273A},
/*' "*/ {0x2827, 0x2822, 0x0000, 0x0000, 0x2827, 0x2827, 0x2822, 0x2822},
/*` ~*/ {0x2960, 0x297E, 0x0000, 0x0000, 0x2960, 0x2960, 0x297E, 0x297E},
/*LShf*/{0x2A00, 0x2A00, 0x2A00, 0x2A00, 0x2A00, 0x2A00, 0x2A00, 0x2A00},
/*\ |*/ {0x2B5C, 0x2B7C, 0x2B1C, 0x0000, 0x2B5C, 0x2B5C, 0x2B7C, 0x2B7C},
/*Z*/   {0x2C7A, 0x2C5A, 0x2C1A, 0x2C00, 0x2C7A, 0x2C5A, 0x2C7A, 0x2C5A},
/*X*/   {0x2D78, 0x2D58, 0x2D18, 0x2D00, 0x2D78, 0x2D58, 0x2D78, 0x2D58},
/*C*/   {0x2E63, 0x2E43, 0x2E03, 0x2E00, 0x2E63, 0x2E43, 0x2E63, 0x2E43},
/*V*/   {0x2F76, 0x2F56, 0x2F16, 0x2F00, 0x2F76, 0x2F56, 0x2F76, 0x2F56},
/*B*/   {0x3062, 0x3042, 0x3002, 0x3000, 0x3062, 0x3042, 0x3062, 0x3042},
/*N*/   {0x316E, 0x314E, 0x310E, 0x3100, 0x316E, 0x314E, 0x316E, 0x314E},
/*M*/   {0x326D, 0x324D, 0x320D, 0x3200, 0x326D, 0x324D, 0x326D, 0x324D},
/*, <*/ {0x332C, 0x333C, 0x0000, 0x0000, 0x332C, 0x332C, 0x333C, 0x333C},
/*. >*/ {0x342E, 0x343E, 0x0000, 0x0000, 0x342E, 0x342E, 0x343E, 0x343E},
/*/ ?*/ {0x352F, 0x353F, 0x0000, 0x0000, 0x352F, 0x352F, 0x353F, 0x353F},
/*rshf*/{0x3600, 0x3600, 0x3600, 0x3600, 0x3600, 0x3600, 0x3600, 0x3600},
/** PS*/{0x372A, 0x0000, 0x3710, 0x0000, 0x372A, 0x372A, 0x0000, 0x0000},
/*alt*/ {0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800},
/*spc*/ {0x3920, 0x3920, 0x3920, 0x0000, 0x3920, 0x3920, 0x3920, 0x3920},
/*caps*/{0x3A00, 0x3A00, 0x3A00, 0x3A00, 0x3A00, 0x3A00, 0x3A00, 0x3A00},
/*F1*/  {0x3B00, 0x5400, 0x5E00, 0x6800, 0x3B00, 0x3B00, 0x5400, 0x5400},
/*F2*/  {0x3C00, 0x5500, 0x5F00, 0x6900, 0x3C00, 0x3C00, 0x5500, 0x5500},
/*F3*/  {0x3D00, 0x5600, 0x6000, 0x6A00, 0x3D00, 0x3D00, 0x5600, 0x5600},
/*F4*/  {0x3E00, 0x5700, 0x6100, 0x6B00, 0x3E00, 0x3E00, 0x5700, 0x5700},
/*F5*/  {0x3F00, 0x5800, 0x6200, 0x6C00, 0x3F00, 0x3F00, 0x5800, 0x5800},
/*F6*/  {0x4000, 0x5900, 0x6300, 0x6D00, 0x4000, 0x4000, 0x5900, 0x5900},
/*F7*/  {0x4100, 0x5A00, 0x6400, 0x6E00, 0x4100, 0x4100, 0x5A00, 0x5A00},
/*F8*/  {0x4200, 0x5B00, 0x6500, 0x6F00, 0x4200, 0x4200, 0x5B00, 0x5B00},
/*F9*/  {0x4300, 0x5C00, 0x6600, 0x7000, 0x4300, 0x4300, 0x5C00, 0x5C00},
/*F10*/ {0x4400, 0x5D00, 0x6700, 0x7100, 0x4400, 0x4400, 0x5D00, 0x5D00},
/*num*/ {0x4500, 0x4500, 0x4500, 0x4500, 0x4500, 0x4500, 0x4500, 0x4500},
/*scrl*/{0x4600, 0x4600, 0x4600, 0x4600, 0x4600, 0x4600, 0x4600, 0x4600},
/*home*/{0x4700, 0x4737, 0x7700, 0x0000, 0x4737, 0x4700, 0x4737, 0x4700},
/*up*/  {0x4800, 0x4838, 0x0000, 0x0000, 0x4838, 0x4800, 0x4838, 0x4800},
/*pgup*/{0x4900, 0x4939, 0x8400, 0x0000, 0x4939, 0x4900, 0x4939, 0x4900},
/*-*/   {0x4A2D, 0x4A2D, 0x0000, 0x0000, 0x4A2D, 0x4A2D, 0x4A2D, 0x4A2D},
/*left*/{0x4B00, 0x4B34, 0x7300, 0x0000, 0x4B34, 0x4B00, 0x4B34, 0x4B00},
/*Cntr*/{0x4C00, 0x4C35, 0x0000, 0x0000, 0x4C35, 0x4C00, 0x4C35, 0x4C00},
/*rght*/{0x4D00, 0x4D36, 0x7400, 0x0000, 0x4D36, 0x4D00, 0x4D36, 0x4D00},
/*+*/   {0x4E2B, 0x4E2B, 0x0000, 0x0000, 0x4E2B, 0x4E2B, 0x4E2B, 0x4E2B},
/*end*/ {0x4F00, 0x4F31, 0x7500, 0x0000, 0x4F31, 0x4F00, 0x4F31, 0x4F00},
/*down*/{0x5000, 0x5032, 0x0000, 0x0000, 0x5032, 0x5000, 0x5032, 0x5000},
/*pgdn*/{0x5100, 0x5133, 0x7600, 0x0000, 0x5133, 0x5100, 0x5133, 0x5100},
/*ins*/ {0x5200, 0x5230, 0x0000, 0x0000, 0x5230, 0x5200, 0x5230, 0x5200},
/*del*/ {0x5300, 0x532E, 0x0000, 0x0000, 0x532E, 0x5300, 0x532E, 0x5300},
/*--*/  {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
/*--*/  {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
/*--*/  {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
/*F11*/ {0x5700, 0x0000, 0x0000, 0x0000, 0x5700, 0x5700, 0x0000, 0x0000},
/*F12*/ {0x5800, 0x0000, 0x0000, 0x0000, 0x5800, 0x5800, 0x0000, 0x0000}
} ;

PRIVATE uint16_t kbd_translate(uint8_t scan)
{
    if (scan == 0xE0 || scan == 0xE1)   return 0x0000 ; /* ignore */
    if (scan & 0x80)                    return scan << 8 ; /* KeyUp */
    if (scan >= ENTRIES(scan_ascii))    return 0x0000 ; /* ignore */

    if (state.alt)  return scan_ascii[scan][3] ;
    if (state.ctrl) return scan_ascii[scan][2] ;
    if (scan >= 0x47)
    {
        if (state.num)
        {
            if (state.lshift || state.rshift)
            {
                return scan_ascii[scan][7] ;
            }
            return scan_ascii[scan][4] ;
        }
    }

    else if (state.caps)
    {
        if (state.lshift || state.rshift)
        {
            return scan_ascii[scan][6] ;
        }
        return scan_ascii[scan][5] ;
    }

    if (state.lshift || state.rshift)
    {
        return scan_ascii[scan][1] ;
    }

    return scan_ascii[scan][0] ;
}

PRIVATE int kbd_set_state(uint8_t scan)
{
    switch (scan)
    {
    case 0x36: state.rshift = TRUE  ; break ;
    case 0xB6: state.rshift = FALSE ; break ;

    case 0x2A: state.lshift = TRUE  ; break ;
    case 0xAA: state.lshift = FALSE ; break ;

    case 0x38: state.alt    = TRUE  ; break ;
    case 0xB8: state.alt    = FALSE ; break ;

    case 0x1D: state.ctrl   = TRUE  ; break ;
    case 0x9D: state.ctrl   = FALSE ; break ;

    case 0x3A: state.caps = !state.caps ;
    case 0xBA: break ;

    case 0x46: state.scrl = !state.scrl ;
    case 0xC6: break ;

    case 0x45: state.num  = !state.num  ;
    case 0xC5: break ;

    case 0x52: state.ins  = !state.ins  ;
    case 0xD2: break ;

    default:   return FALSE ;
    }

    return TRUE ;
}

#define PORT_KBD_STS 0x64
#define      KBD_STS_RDY  0x01
#define PORT_KBD_DAT 0x60

/*键盘缓冲区*/
static uint16_t buf_kbd;

/*等待键盘输入的线程队列*/
static struct wait_queue *wq_kbd = NULL;

/**
 * 键盘的中断处理程序
 */
void isr_keyboard(uint32_t irq, struct context *ctx)
{
    /*再次确认用户是否按键*/
    if(inportb(PORT_KBD_STS) & KBD_STS_RDY) {
        uint8_t scan;
        uint16_t ascii;

        /*是的。把按键的扫描码读进来*/
        scan = inportb(PORT_KBD_DAT);

        /*是Ctrl/Alt/Shift/Caps lock/等等吗？*/
        if(kbd_set_state(scan))
            return;

        /*把键盘扫描码转为ASCII码*/
        if((ascii = kbd_translate(scan)) == 0)
            return;

        /*放到键盘缓冲区*/
        buf_kbd = ascii;

        /*唤醒一个等待键盘输入的线程*/
        wake_up(&wq_kbd, 1);
    }
}

/**
 * 系统调用getchar的执行函数
 *
 * 从键盘上读入一个字符，如果没有按键则等待
 */
int sys_getchar()
{
    uint32_t flags;

    /*等待键盘输入，亦即在队列wq_kbd上等待*/
    save_flags_cli(flags);
    sleep_on(&wq_kbd);
    restore_flags(flags);

    return buf_kbd;
}
