#include <stdio.h>
#include <string.h>
#include <initializer_list>
#include "_bus_hardware.cpp"

static unsigned heartbeats;
void bambubus_heartbeat_seen_fast() { ++heartbeats; }
#define CHECK(condition) do { if (!(condition)) { \
    printf("line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

static uint8_t request[] = {0x3d, 0xc5, 8, 0, 0x21, 0, 0, 0};
static void interrupt()
{
    const uint32_t state = irq_save_wch();
    USART1_IRQHandler();
    irq_restore_wch(state);
}
static void receive(uint8_t byte, uint32_t errors = 0)
{
    USART1->DATAR.value = byte;
    USART1->STATR |= USART_FLAG_RXNE | errors;
    interrupt();
}
static void frame() { for (uint8_t b : request) receive(b); }
static void release()
{
    const uint32_t state = irq_save_wch();
    bus_port_to_host.recv_data_len = 0;
    bus_port_to_host.bus_package_type = _bus_data_type::none;
    irq_restore_wch(state);
}
static void send()
{
    memcpy(bus_port_to_host.tx_build_buf(), request, sizeof(request));
    bus_port_to_host.send_data_len = sizeof(request);
    bus_port_to_host.send_package();
}
static bool stopped()
{
    return bus_port_to_host.idle && !GPIOA->de &&
        !(DMA1_Channel4->CFGR & DMA_CFGR1_EN) &&
        !(USART1->CTLR3 & USART_DMAReq_Tx) &&
        !(DMA1->INTFR & (DMA1_FLAG_TC4 | DMA1_FLAG_HT4 | DMA1_FLAG_TE4)) &&
        !(USART1->STATR & USART_IT_TC);
}

int main()
{
    bus_init();
    request[3] = bus_crc8(request, 3);
    CHECK(USART1->enabled_interrupts & USART_IT_RXNE);
    CHECK(USART1->enabled_interrupts & USART_IT_ERR);
    CHECK(USART1->enabled_interrupts & USART_IT_PE);
    CHECK(!(USART1->CTLR3 & USART_DMAReq_Rx));
    CHECK(DMA1_Channel5->CNTR == 0);

    // RXNE still publishes frames immediately; no main-loop RX polling.
    frame();
    CHECK(bus_port_to_host.recv_data_len == 8);
    uint8_t* published = bus_port_to_host.bus_recv_data_ptr;
    CHECK(memcmp(published, request, 8) == 0);
    // An error in the other buffer must not invalidate the published frame.
    receive(0x3d); receive(0xc5, USART_FLAG_FE);
    CHECK(bus_port_to_host.bus_recv_data_ptr == published);
    CHECK(bus_port_to_host.recv_data_len == 8);
    CHECK(memcmp(published, request, 8) == 0);
    release();

    for (uint32_t error : {USART_FLAG_ORE, USART_FLAG_NE, USART_FLAG_FE, USART_FLAG_PE})
    {
        receive(0x3d); receive(0xc5);
        const unsigned reads = USART1->DATAR.reads;
        receive(0x3d, error); // The corrupt sync byte must not start another frame.
        CHECK(USART1->DATAR.reads == reads + 1);
        CHECK(!(USART1->STATR & (15u | USART_FLAG_RXNE)));
        frame();
        CHECK(bus_port_to_host.recv_data_len == 8);
        CHECK(memcmp(bus_port_to_host.bus_recv_data_ptr, request, 8) == 0);
        release();
    }
    // Error without RXNE still clears through one status/data read sequence.
    receive(0x3d);
    USART1->STATR |= USART_FLAG_ORE;
    interrupt();
    CHECK(!(USART1->STATR & USART_FLAG_ORE));
    frame(); CHECK(bus_port_to_host.recv_data_len == 8); release();

    // A damaged fast heartbeat must not survive in the parser's skip state.
    request[4] = 0x20;
    for (unsigned i = 0; i < 5; ++i) receive(request[i]);
    receive(0, USART_FLAG_NE);
    receive(0); receive(0); receive(0);
    CHECK(heartbeats == 0);
    frame(); CHECK(heartbeats == 1);
    request[4] = 0x21;

    // Startup clears stale TX flags, leaves other DMA channels alone and sends now.
    DMA1->INTFR = DMA1_FLAG_TE4 | DMA1_FLAG_TC4 | DMA1_FLAG_HT4 | DMA1_FLAG_TE5;
    USART1->STATR |= USART_IT_TC;
    send();
    CHECK(!bus_port_to_host.idle && GPIOA->de);
    CHECK(DMA1_Channel4->CNTR == 8 && bus_port_to_host.send_data_len == 0);
    CHECK(DMA1->INTFR == DMA1_FLAG_TE5);
    CHECK(!(USART1->STATR & USART_IT_TC));
    frame(); CHECK(bus_port_to_host.recv_data_len == 0); // Existing TX echo policy.
    bus_uart1_tx_poll(); CHECK(!bus_port_to_host.idle);
    DMA1->INTFR |= DMA1_FLAG_TE4;
    bus_uart1_tx_poll(); CHECK(stopped());
    CHECK(DMA1->INTFR == DMA1_FLAG_TE5);

    // Timeout is measured from TX start and works across the 32-bit tick wrap.
    test_tick = 0xfffffff0u;
    send(); test_tick += 24999;
    bus_uart1_tx_poll(); CHECK(!bus_port_to_host.idle);
    ++test_tick; bus_uart1_tx_poll(); CHECK(stopped());
    send();
    // Simultaneous RX error, TX DMA error and TC must still release the bus.
    DMA1->INTFR |= DMA1_FLAG_TE4;
    USART1->STATR |= USART_IT_TC | USART_FLAG_ORE;
    interrupt(); CHECK(stopped());

    // A completed TX cannot be aborted again by a later timeout poll.
    send(); USART1->STATR |= USART_IT_TC; interrupt(); CHECK(stopped());
    test_tick += 25000; bus_uart1_tx_poll(); CHECK(stopped());
    // New receive/transmit still work after all recovery paths.
    frame(); CHECK(bus_port_to_host.recv_data_len == 8);
    // Sending does not flush a complete, already published RX frame.
    send(); CHECK(bus_port_to_host.recv_data_len == 8);
    CHECK(memcmp(bus_port_to_host.bus_recv_data_ptr, request, 8) == 0);
    release();
    USART1->STATR |= USART_IT_TC; interrupt(); CHECK(stopped());
    CHECK(test_irq_state == 0x88);
    return 0;
}
