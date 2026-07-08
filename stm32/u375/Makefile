# STM32U375CGU6Q bare metal Makefile (HAL/Cube 미사용)
# PlatformIO 내장 arm-none-eabi 툴체인 재활용

# ---- 툴체인 경로 ----
TC   := $(HOME)/.platformio/packages/toolchain-gccarmnoneeabi/bin
CC   := $(TC)/arm-none-eabi-gcc
OBJCOPY := $(TC)/arm-none-eabi-objcopy
SIZE := $(TC)/arm-none-eabi-size

# ---- 대상 ----
TARGET := blink
BUILD  := build

# ---- 소스 ----
C_SRC   := src/main.c
ASM_SRC := startup_stm32u375.s
LDSCRIPT := STM32U375CG_FLASH.ld

# ---- 플래그 (Cortex-M33, FPU는 blink에 불필요하여 soft-abi) ----
CPU := -mcpu=cortex-m33 -mthumb -mfloat-abi=soft
CFLAGS  := $(CPU) -Wall -Wextra -O2 -ffreestanding -ffunction-sections -fdata-sections -g
ASFLAGS := $(CPU) -g
LDFLAGS := $(CPU) -T$(LDSCRIPT) -nostartfiles -nostdlib \
           -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map

OBJS := $(BUILD)/main.o $(BUILD)/startup.o

# ---- 규칙 ----
all: $(BUILD)/$(TARGET).bin

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/main.o: $(C_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/startup.o: $(ASM_SRC) | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(OBJCOPY) -O ihex   $< $(BUILD)/$(TARGET).hex

# ---- 업로드 (openocd SWD) ----
# ⚠️ 설치된 openocd 0.12.0(2023)은 STM32U3 미지원(stm32u3x.cfg 없음).
#    아래 stm32u5x.cfg는 미검증 우회 — 보드 실물 오면 실패 시 최신 openocd
#    또는 STM32CubeProgrammer CLI 로 교체할 것.
OCD     := $(HOME)/.platformio/packages/tool-openocd/bin/openocd
OCD_CFG := $(HOME)/.platformio/packages/tool-openocd/openocd/scripts
flash: $(BUILD)/$(TARGET).elf
	$(OCD) -s $(OCD_CFG) -f interface/stlink.cfg -f target/stm32u5x.cfg \
	       -c "program $< verify reset exit"

clean:
	rm -rf $(BUILD)

.PHONY: all flash clean
