/*
 * STM32U375CGU6Q linker script (bare metal)
 * 메모리 크기는 CMSIS stm32u375xx.h 원본에서 검증:
 *   FLASH = 1MB  @ 0x08000000  (FLASH_SIZE 기본 0x100000)
 *   SRAM1 = 192KB @ 0x20000000, SRAM2 = 64KB @ 0x20030000  → 연속 256KB
 */

/* 스택 top = RAM 끝 (0x20000000 + 0x40000) */
_estack = 0x20040000;

_Min_Heap_Size  = 0x200;   /* 최소 힙 (여기선 미사용에 가깝지만 관례상 확보) */
_Min_Stack_Size = 0x400;   /* 최소 스택 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
    RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 256K
}

SECTIONS
{
    /* 벡터 테이블은 flash 맨 앞 */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    /* 코드 */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } >FLASH

    /* 읽기 전용 데이터 */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

    .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } >FLASH

    /* .data 초기값이 저장된 flash 주소 (startup에서 _sidata로 복사) */
    _sidata = LOADADDR(.data);

    /* 초기화된 전역변수: flash에 값 저장, 실행 시 RAM으로 복사 */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >RAM AT> FLASH

    /* 0으로 초기화되는 전역변수 */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >RAM

    /* 힙/스택 여유 공간 확인용 (넘치면 링크 에러) */
    ._user_heap_stack :
    {
        . = ALIGN(8);
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } >RAM

    /DISCARD/ : { libc.a ( * ) libm.a ( * ) libgcc.a ( * ) }
    .ARM.attributes 0 : { *(.ARM.attributes) }
}
