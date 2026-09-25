#pragma once
#include <stdint.h>

// Host model of the registers touched by the production printer transport.
// It models DMA write-one-to-clear flags and USART status/data error clearing.
constexpr uint32_t DMA1_FLAG_GL4 = 0x1000, DMA1_FLAG_TC4 = 0x2000;
constexpr uint32_t DMA1_FLAG_HT4 = 0x4000, DMA1_FLAG_TE4 = 0x8000;
constexpr uint32_t DMA1_FLAG_GL5 = 0x10000, DMA1_FLAG_TC5 = 0x20000;
constexpr uint32_t DMA1_FLAG_HT5 = 0x40000, DMA1_FLAG_TE5 = 0x80000;
constexpr uint32_t USART_FLAG_PE = 1, USART_FLAG_FE = 2;
constexpr uint32_t USART_FLAG_NE = 4, USART_FLAG_ORE = 8;
constexpr uint32_t USART_IT_TC = 0x40, USART_IT_RXNE = 0x20;
constexpr uint32_t USART_FLAG_RXNE = 0x20;
constexpr uint32_t USART_IT_ERR = 0x100, USART_IT_PE = 0x200;
constexpr uint32_t USART_DMAReq_Rx = 0x40, USART_DMAReq_Tx = 0x80;
constexpr uint32_t DMA_CFGR1_EN = 1;
constexpr uint32_t DMA_IT_HT = 4, DMA_IT_TC = 2, DMA_IT_TE = 8;
constexpr uint32_t GPIO_Pin_9 = 1 << 9, GPIO_Pin_10 = 1 << 10, GPIO_Pin_12 = 1 << 12;
constexpr uint32_t ENABLE = 1, DISABLE = 0, RESET = 0;
constexpr uint32_t DMA_Mode_Normal = 0, DMA_Mode_Circular = 0x20;
constexpr uint32_t RCC_AHBPeriph_CRC = 0, RCC_AHBPeriph_DMA1 = 0;
constexpr uint32_t RCC_APB2Periph_USART1 = 0, RCC_APB2Periph_GPIOA = 0;
constexpr uint32_t GPIO_Speed_50MHz = 0, GPIO_Mode_AF_PP = 0;
constexpr uint32_t GPIO_Mode_IPU = 0, GPIO_Mode_Out_PP = 0;
constexpr uint32_t USART_WordLength_9b = 0, USART_StopBits_1 = 0;
constexpr uint32_t USART_Parity_Even = 0, USART_HardwareFlowControl_None = 0;
constexpr uint32_t USART_Mode_Tx = 0, USART_Mode_Rx = 0;
constexpr uint32_t USART1_IRQn = 0, DMA1_Channel5_IRQn = 0;
constexpr uint32_t DMA_DIR_PeripheralDST = 0, DMA_DIR_PeripheralSRC = 0;
constexpr uint32_t DMA_PeripheralInc_Disable = 0, DMA_MemoryInc_Enable = 0;
constexpr uint32_t DMA_Priority_VeryHigh = 0, DMA_M2M_Disable = 0;
constexpr uint32_t DMA_MemoryDataSize_Byte = 0, DMA_PeripheralDataSize_Byte = 0;

static uint32_t test_tick = 1, test_irq_state = 0x88;
uint32_t time_hw_tpus = 1, time_hw_tpms = 1000;
inline uint32_t time_ticks32() { return test_tick; }
inline int32_t time_diff32(uint32_t a, uint32_t b) { return (int32_t)(a - b); }
inline uint32_t irq_save_wch() { uint32_t s = test_irq_state; test_irq_state = 0; return s; }
inline void irq_restore_wch(uint32_t s) { test_irq_state = s; }

struct TestDma
{
    uint32_t INTFR = 0;
    struct Clear
    {
        uint32_t* flags;
        void operator=(uint32_t v) { *flags &= ~v; }
    } INTFCR{&INTFR};
};
struct TestChannel { uint32_t CFGR = 0, MADDR = 0, CNTR = 0; };
struct TestUsart
{
    uint32_t STATR = 0, CTLR3 = 0, enabled_interrupts = 0;
    struct Data
    {
        uint32_t* status;
        uint8_t value = 0;
        unsigned reads = 0;
        explicit Data(uint32_t* s) : status(s) {}
        operator uint32_t() { ++reads; *status &= ~(15u | USART_FLAG_RXNE); return value; }
    } DATAR{&STATR};
};
struct TestGpio
{
    bool de = false;
    struct Set { bool* de; void operator=(uint32_t) { *de = true; } } BSHR{&de};
    struct Clear { bool* de; void operator=(uint32_t) { *de = false; } } BCR{&de};
};
static TestDma test_dma;
static TestChannel test_rx, test_tx;
static TestUsart test_usart;
static TestGpio test_gpio;
#define DMA1 (&test_dma)
#define DMA1_Channel4 (&test_tx)
#define DMA1_Channel5 (&test_rx)
#define USART1 (&test_usart)
#define GPIOA (&test_gpio)

struct GPIO_InitTypeDef { uint32_t GPIO_Pin, GPIO_Speed, GPIO_Mode; };
struct USART_InitTypeDef
{
    uint32_t USART_BaudRate, USART_WordLength, USART_StopBits, USART_Parity;
    uint32_t USART_HardwareFlowControl, USART_Mode;
};
struct NVIC_InitTypeDef
{
    uint32_t NVIC_IRQChannel, NVIC_IRQChannelPreemptionPriority;
    uint32_t NVIC_IRQChannelSubPriority, NVIC_IRQChannelCmd;
};
struct DMA_InitTypeDef
{
    uint32_t DMA_PeripheralBaseAddr, DMA_MemoryBaseAddr, DMA_DIR, DMA_Mode;
    uint32_t DMA_PeripheralInc, DMA_MemoryInc, DMA_Priority, DMA_M2M;
    uint32_t DMA_MemoryDataSize, DMA_PeripheralDataSize, DMA_BufferSize;
};
inline void RCC_AHBPeriphClockCmd(uint32_t, uint32_t) {}
inline void RCC_APB2PeriphClockCmd(uint32_t, uint32_t) {}
inline void GPIO_Init(TestGpio*, GPIO_InitTypeDef*) {}
inline void USART_Init(TestUsart*, USART_InitTypeDef*) {}
inline void USART_Cmd(TestUsart*, uint32_t) {}
inline void NVIC_Init(NVIC_InitTypeDef*) {}
inline void USART_ITConfig(TestUsart* u, uint32_t bit, uint32_t enabled)
{ if (enabled) u->enabled_interrupts |= bit; else u->enabled_interrupts &= ~bit; }
inline uint32_t USART_GetITStatus(TestUsart* u, uint32_t bit)
{ return u->STATR & u->enabled_interrupts & bit; }
inline void USART_ClearITPendingBit(TestUsart* u, uint32_t bit) { u->STATR &= ~bit; }
inline uint32_t USART_ReceiveData(TestUsart* u) { return u->DATAR; }
inline void USART_DMACmd(TestUsart* u, uint32_t bit, uint32_t enabled)
{ if (enabled) u->CTLR3 |= bit; else u->CTLR3 &= ~bit; }
inline void DMA_DeInit(TestChannel* ch) { ch->CFGR = ch->CNTR = 0; }
inline void DMA_Init(TestChannel* ch, DMA_InitTypeDef* cfg)
{ ch->CFGR = cfg->DMA_Mode; ch->MADDR = cfg->DMA_MemoryBaseAddr; ch->CNTR = cfg->DMA_BufferSize; }
inline void DMA_Cmd(TestChannel* ch, uint32_t enabled)
{ if (enabled) ch->CFGR |= DMA_CFGR1_EN; else ch->CFGR &= ~DMA_CFGR1_EN; }
inline void DMA_ITConfig(TestChannel*, uint32_t, uint32_t) {}
