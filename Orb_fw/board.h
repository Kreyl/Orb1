#pragma once

// ==== General ====
#define BOARD_NAME          "Orb1"
#define APP_NAME            "Orb1"

#ifndef TRUE
#define TRUE    1
#endif

// ==== High-level peripery control ====
#define PILL_ENABLED        FALSE
#define BEEPER_ENABLED      FALSE
#define BUTTONS_ENABLED     TRUE

#define SIMPLESENSORS_ENABLED   TRUE

// MCU type as defined in the ST header.
#define STM32L151xB

// Freq of external crystal if any. Leave it here even if not used.
#define CRYSTAL_FREQ_HZ 12000000

// OS timer settings
#define STM32_ST_IRQ_PRIORITY   2
#define STM32_ST_USE_TIMER      2
#define STM32_TIMCLK1           (Clk.APB1FreqHz)

#define ADC_REQUIRED    TRUE

// LEDs config: BandCnt; {LedCnt1, LedCnt2, LedCnt3...}
#define BAND_CNT        1
#define BAND_SETUPS     ((const BandSetup_t[]){\
    {18, dirForward}, \
    })

#if 1 // ========================== GPIO =======================================
// PortMinTim_t: GPIO, Pin, Tim, TimChnl, invInverted, omPushPull, TopValue
// UART
#define UART_GPIO       GPIOA
#define UART_TX_PIN     9
#define UART_RX_PIN     10

// Buttons
#define BTN1_PIN        GPIOA, 0, pudPullDown
#define BTN2_PIN        GPIOA, 1, pudPullDown
#define BTN3_PIN        GPIOA, 2, pudPullDown

// Charging & USB
#define IS_CHARGING_PIN GPIOB, 3
#define PIN_5V_USB      GPIOC, 13

// Npx LEDs
#define NPX_SPI         SPI1
#define NPX_DATA_PIN    GPIOA, 7, AF5
#define NPX_PWR_PIN     GPIOA, 8
//#define NPX_GENER_PIN   GPIOA, 3, TIM9, 2, invNotInverted, omPushPull, 255

// ADC
#define ADC_BAT_EN      GPIOB, 2
#define ADC_BAT_PIN     GPIOB, 1

#endif // GPIO

#if ADC_REQUIRED // ======================= Inner ADC ==========================
#define BAT_CHNL            9
#define ADC_VREFINT_CHNL    17  // All 4xx, F072 and L151 devices. Do not change.
#define ADC_CHANNELS        { BAT_CHNL, ADC_VREFINT_CHNL }
#define ADC_CHANNEL_CNT     2   // Do not use countof(AdcChannels) as preprocessor does not know what is countof => cannot check
#define ADC_SAMPLE_TIME     ast96Cycles
#define ADC_SAMPLE_CNT      8   // How many times to measure every channel
#define ADC_SEQ_LEN         (ADC_SAMPLE_CNT * ADC_CHANNEL_CNT)
#endif

#if 1 // =========================== DMA =======================================
#define STM32_DMA_REQUIRED  TRUE
// ==== Uart ====
#define UART_DMA_TX_MODE(Chnl) (STM32_DMA_CR_CHSEL(Chnl) | DMA_PRIORITY_LOW | STM32_DMA_CR_MSIZE_BYTE | STM32_DMA_CR_PSIZE_BYTE | STM32_DMA_CR_MINC | STM32_DMA_CR_DIR_M2P | STM32_DMA_CR_TCIE)
#define UART_DMA_RX_MODE(Chnl) (STM32_DMA_CR_CHSEL(Chnl) | DMA_PRIORITY_MEDIUM | STM32_DMA_CR_MSIZE_BYTE | STM32_DMA_CR_PSIZE_BYTE | STM32_DMA_CR_MINC | STM32_DMA_CR_DIR_P2M | STM32_DMA_CR_CIRC)
#define UART_DMA_TX     STM32_DMA_STREAM_ID(1, 4)
#define UART_DMA_RX     STM32_DMA_STREAM_ID(1, 5)
#define UART_DMA_CHNL   0   // Dummy

// ==== Npx ====
#define NPX_DMA         STM32_DMA_STREAM_ID(1, 3)  // SPI1 TX

#if ADC_REQUIRED
#define ADC_DMA         STM32_DMA_STREAM_ID(1, 1)
#define ADC_DMA_MODE    STM32_DMA_CR_CHSEL(0) |   /* dummy */ \
                        DMA_PRIORITY_LOW | \
                        STM32_DMA_CR_MSIZE_HWORD | \
                        STM32_DMA_CR_PSIZE_HWORD | \
                        STM32_DMA_CR_MINC |       /* Memory pointer increase */ \
                        STM32_DMA_CR_DIR_P2M |    /* Direction is peripheral to memory */ \
                        STM32_DMA_CR_TCIE         /* Enable Transmission Complete IRQ */
#endif // ADC

#endif // DMA

#if 1 // ========================== USART ======================================
#define PRINTF_FLOAT_EN FALSE
#define UART_TXBUF_SZ   512
#define UART_RXBUF_SZ   1024
#define CMD_BUF_SZ      128

#define CMD_UART        USART1

#define UARTS_CNT       1

#define CMD_UART_PARAMS \
    CMD_UART, UART_GPIO, UART_TX_PIN, UART_GPIO, UART_RX_PIN, \
    UART_DMA_TX, UART_DMA_RX, UART_DMA_TX_MODE(UART_DMA_CHNL), UART_DMA_RX_MODE(UART_DMA_CHNL)

#endif
