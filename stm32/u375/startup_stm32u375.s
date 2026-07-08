/*
 * STM32U375CGU6Q (Cortex-M33) 최소 startup + 벡터테이블
 * - 초기 SP = _estack (linker에서), 리셋 시 .data 복사 / .bss 클리어 후 main() 호출
 * - 시스템 예외 16개만 정의, 주변장치 IRQ는 공용 Default_Handler로 처리(최소 예제)
 */
    .syntax unified
    .cpu cortex-m33
    .thumb

    .global g_pfnVectors
    .global Default_Handler

/* linker.ld 심볼 */
    .word _sidata      /* .data 초기값이 저장된 flash 주소 */
    .word _sdata       /* .data 시작 (RAM)                 */
    .word _edata       /* .data 끝   (RAM)                 */
    .word _sbss        /* .bss  시작                       */
    .word _ebss        /* .bss  끝                         */

/* =================== Reset Handler =================== */
    .section .text.Reset_Handler
    .weak Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:
    ldr   sp, =_estack           /* 스택 포인터 설정 */

    /* .data 섹션 복사 (flash -> RAM) */
    ldr   r0, =_sdata
    ldr   r1, =_edata
    ldr   r2, =_sidata
    movs  r3, #0
    b     LoopCopyData
CopyData:
    ldr   r4, [r2, r3]
    str   r4, [r0, r3]
    adds  r3, r3, #4
LoopCopyData:
    adds  r4, r0, r3
    cmp   r4, r1
    bcc   CopyData

    /* .bss 섹션 0으로 초기화 */
    ldr   r2, =_sbss
    ldr   r4, =_ebss
    movs  r3, #0
    b     LoopFillZero
FillZero:
    str   r3, [r2]
    adds  r2, r2, #4
LoopFillZero:
    cmp   r2, r4
    bcc   FillZero

    bl    main                   /* main() 호출 */
LoopForever:
    b     LoopForever            /* main이 리턴하면 여기 정지 */
    .size Reset_Handler, .-Reset_Handler

/* =================== Default Handler =================== */
    .section .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
    b     Infinite_Loop
    .size Default_Handler, .-Default_Handler

/* =================== 벡터 테이블 =================== */
    .section .isr_vector,"a",%progbits
    .type g_pfnVectors, %object
g_pfnVectors:
    .word _estack               /* 0x00 초기 스택 포인터 */
    .word Reset_Handler         /* 0x04 리셋 */
    .word NMI_Handler           /* 0x08 */
    .word HardFault_Handler     /* 0x0C */
    .word MemManage_Handler     /* 0x10 */
    .word BusFault_Handler      /* 0x14 */
    .word UsageFault_Handler    /* 0x18 */
    .word SecureFault_Handler   /* 0x1C (Cortex-M33 TrustZone) */
    .word 0                     /* 0x20 reserved */
    .word 0                     /* 0x24 reserved */
    .word 0                     /* 0x28 reserved */
    .word SVC_Handler           /* 0x2C */
    .word DebugMon_Handler      /* 0x30 */
    .word 0                     /* 0x34 reserved */
    .word PendSV_Handler        /* 0x38 */
    .word SysTick_Handler       /* 0x3C */
    /* 주변장치 IRQ는 최소 예제라 생략 (필요 시 여기 아래로 추가) */
    .size g_pfnVectors, .-g_pfnVectors

/* 모든 시스템 예외 핸들러를 Default_Handler로 약하게 별칭 (재정의 가능) */
    .macro  weak_alias name
    .weak   \name
    .thumb_set \name, Default_Handler
    .endm

    weak_alias NMI_Handler
    weak_alias HardFault_Handler
    weak_alias MemManage_Handler
    weak_alias BusFault_Handler
    weak_alias UsageFault_Handler
    weak_alias SecureFault_Handler
    weak_alias SVC_Handler
    weak_alias DebugMon_Handler
    weak_alias PendSV_Handler
    weak_alias SysTick_Handler
