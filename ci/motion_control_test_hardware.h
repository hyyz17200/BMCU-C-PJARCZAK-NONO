// Host model of what src/Motion_control.cpp needs from the WCH SDK, SysTick and the LED driver.
// ci/test_unload_hold.py makes ch32v20x.h, ch32v20x_gpio.h, ws2812.h and hal/time_hw.h include this file.
// Motor PWM compare writes are kept so the test can read each channel's drive back.
#pragma once
#include <stdint.h>
#include <stdlib.h>

typedef struct { uint32_t unused; } GPIO_TypeDef;
typedef struct { uint32_t unused; } TIM_TypeDef;

extern GPIO_TypeDef hw_gpio[4];
extern TIM_TypeDef  hw_tim[5];
extern uint16_t     hw_ccr[5][5]; // [timer][compare channel]

#define GPIOA (&hw_gpio[0])
#define GPIOB (&hw_gpio[1])
#define GPIOC (&hw_gpio[2])
#define GPIOD (&hw_gpio[3])
#define TIM2  (&hw_tim[2])
#define TIM3  (&hw_tim[3])
#define TIM4  (&hw_tim[4])

#define GPIO_Pin_0  ((uint16_t)0x0001)
#define GPIO_Pin_1  ((uint16_t)0x0002)
#define GPIO_Pin_2  ((uint16_t)0x0004)
#define GPIO_Pin_3  ((uint16_t)0x0008)
#define GPIO_Pin_4  ((uint16_t)0x0010)
#define GPIO_Pin_5  ((uint16_t)0x0020)
#define GPIO_Pin_6  ((uint16_t)0x0040)
#define GPIO_Pin_7  ((uint16_t)0x0080)
#define GPIO_Pin_8  ((uint16_t)0x0100)
#define GPIO_Pin_9  ((uint16_t)0x0200)
#define GPIO_Pin_10 ((uint16_t)0x0400)
#define GPIO_Pin_11 ((uint16_t)0x0800)
#define GPIO_Pin_12 ((uint16_t)0x1000)
#define GPIO_Pin_13 ((uint16_t)0x2000)
#define GPIO_Pin_14 ((uint16_t)0x4000)
#define GPIO_Pin_15 ((uint16_t)0x8000)

typedef enum { DISABLE = 0, ENABLE = 1 } FunctionalState;
typedef enum { GPIO_Speed_10MHz = 1, GPIO_Speed_2MHz, GPIO_Speed_50MHz } GPIOSpeed_TypeDef;
typedef enum { GPIO_Mode_AIN = 0x0, GPIO_Mode_AF_PP = 0x18 } GPIOMode_TypeDef;
typedef struct { uint16_t GPIO_Pin; GPIOSpeed_TypeDef GPIO_Speed; GPIOMode_TypeDef GPIO_Mode; } GPIO_InitTypeDef;
typedef struct
{
    uint16_t TIM_Prescaler, TIM_CounterMode, TIM_Period, TIM_ClockDivision;
    uint8_t  TIM_RepetitionCounter;
} TIM_TimeBaseInitTypeDef;
typedef struct
{
    uint16_t TIM_OCMode, TIM_OutputState, TIM_OutputNState, TIM_Pulse;
    uint16_t TIM_OCPolarity, TIM_OCNPolarity, TIM_OCIdleState, TIM_OCNIdleState;
} TIM_OCInitTypeDef;

#define TIM_CounterMode_Up     ((uint16_t)0x0000)
#define TIM_OCMode_PWM1        ((uint16_t)0x0060)
#define TIM_OutputState_Enable ((uint16_t)0x0001)
#define TIM_OCPolarity_High    ((uint16_t)0x0000)
#define TIM_OCPreload_Enable   ((uint16_t)0x0008)
#define RCC_APB2Periph_AFIO    ((uint32_t)0x01)
#define RCC_APB2Periph_GPIOA   ((uint32_t)0x04)
#define RCC_APB2Periph_GPIOB   ((uint32_t)0x08)
#define RCC_APB2Periph_GPIOC   ((uint32_t)0x10)
#define RCC_APB2Periph_GPIOD   ((uint32_t)0x20)
#define RCC_APB1Periph_TIM2    ((uint32_t)0x01)
#define RCC_APB1Periph_TIM3    ((uint32_t)0x02)
#define RCC_APB1Periph_TIM4    ((uint32_t)0x04)
#define GPIO_FullRemap_TIM2    ((uint32_t)1)
#define GPIO_PartialRemap_TIM3 ((uint32_t)2)
#define GPIO_Remap_TIM4        ((uint32_t)3)

static inline void GPIO_Init(GPIO_TypeDef*, GPIO_InitTypeDef*) {}
static inline void GPIO_PinRemapConfig(uint32_t, FunctionalState) {}
static inline void RCC_APB1PeriphClockCmd(uint32_t, FunctionalState) {}
static inline void RCC_APB2PeriphClockCmd(uint32_t, FunctionalState) {}
static inline void TIM_TimeBaseInit(TIM_TypeDef*, TIM_TimeBaseInitTypeDef*) {}
static inline void TIM_OC1Init(TIM_TypeDef*, TIM_OCInitTypeDef*) {}
static inline void TIM_OC2Init(TIM_TypeDef*, TIM_OCInitTypeDef*) {}
static inline void TIM_OC3Init(TIM_TypeDef*, TIM_OCInitTypeDef*) {}
static inline void TIM_OC4Init(TIM_TypeDef*, TIM_OCInitTypeDef*) {}
static inline void TIM_OC1PreloadConfig(TIM_TypeDef*, uint16_t) {}
static inline void TIM_OC2PreloadConfig(TIM_TypeDef*, uint16_t) {}
static inline void TIM_OC3PreloadConfig(TIM_TypeDef*, uint16_t) {}
static inline void TIM_OC4PreloadConfig(TIM_TypeDef*, uint16_t) {}
static inline void TIM_CtrlPWMOutputs(TIM_TypeDef*, FunctionalState) {}
static inline void TIM_ARRPreloadConfig(TIM_TypeDef*, FunctionalState) {}
static inline void TIM_Cmd(TIM_TypeDef*, FunctionalState) {}
static inline void TIM_SetCompare1(TIM_TypeDef* t, uint16_t v) { hw_ccr[t - hw_tim][1] = v; }
static inline void TIM_SetCompare2(TIM_TypeDef* t, uint16_t v) { hw_ccr[t - hw_tim][2] = v; }
static inline void TIM_SetCompare3(TIM_TypeDef* t, uint16_t v) { hw_ccr[t - hw_tim][3] = v; }
static inline void TIM_SetCompare4(TIM_TypeDef* t, uint16_t v) { hw_ccr[t - hw_tim][4] = v; }
static inline void NVIC_SystemReset(void) { abort(); }

// hal/time_hw.h: SysTick is a tick counter the test advances.
#ifdef __cplusplus
extern "C" {
#endif
extern uint32_t time_hw_tpus;
extern uint32_t time_hw_tpms;
extern uint64_t hw_ticks;
static inline uint32_t time_hw_ticks_per_ms(void) { return time_hw_tpms; }
static inline uint32_t time_ticks32(void) { return (uint32_t)hw_ticks; }
static inline uint64_t time_ticks64(void) { return hw_ticks; }
void delay(uint32_t ms);
void delay_us(uint32_t us);
#ifdef __cplusplus
}
#endif

// ws2812.h: the LEDs are not observed.
#ifndef BMCU_ONLINE_LED_FILAMENT_RGB
#define BMCU_ONLINE_LED_FILAMENT_RGB 0
#endif
class WS2812_class
{
public:
    void set_RGB(uint8_t, uint8_t, uint8_t, uint8_t) {}
    void set_RGB_online(uint8_t, uint8_t, uint8_t, uint8_t, bool = false) {}
    bool is_dirty() const { return false; }
    void updata() {}
};
