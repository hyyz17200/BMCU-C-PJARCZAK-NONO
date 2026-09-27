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
// Model the enabled IRQ sources, including ORE's level request through RXNEIE.
// Flags are injected by the test; this does not prove how silicon reaches them.
static bool irq_pending()
{
    const uint32_t status = USART1->STATR;
    const uint32_t enabled = USART1->enabled_interrupts;
    return ((enabled & USART_IT_RXNE) && (status & (USART_FLAG_RXNE | USART_FLAG_ORE))) ||
           ((enabled & USART_IT_TC) && (status & USART_IT_TC));
}
static unsigned service_pending_interrupts()
{
    unsigned calls = 0;
    while (irq_pending() && calls < 8u) // Bound a broken driver's IRQ storm in the test.
    {
        interrupt();
        ++calls;
    }
    return calls;
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
    CHECK(!(USART1->enabled_interrupts & USART_IT_ERR));
    CHECK(!(USART1->enabled_interrupts & USART_IT_PE));
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

    for (uint32_t error : {USART_FLAG_ORE, USART_FLAG_FE, USART_FLAG_PE})
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
    // A noise flag keeps the voted byte: the frame is published and NE is cleared.
    for (unsigned i = 0; i < sizeof(request); ++i)
        receive(request[i], i == 5 ? USART_FLAG_NE : 0u);
    CHECK(!(USART1->STATR & (15u | USART_FLAG_RXNE)));
    CHECK(bus_port_to_host.recv_data_len == 8);
    CHECK(memcmp(bus_port_to_host.bus_recv_data_ptr, request, 8) == 0);
    release();
    // Error without RXNE still clears through one status/data read sequence.
    receive(0x3d);
    USART1->STATR |= USART_FLAG_ORE;
    interrupt();
    CHECK(!(USART1->STATR & USART_FLAG_ORE));
    frame(); CHECK(bus_port_to_host.recv_data_len == 8); release();

    // Error status can precede RXNE. Even a simultaneous TX completion must
    // not read stale DATAR or clear the pending error before its byte arrives.
    for (uint32_t early_error : {USART_FLAG_PE, USART_FLAG_PE | USART_FLAG_NE, USART_FLAG_FE})
    for (bool tx_complete : {false, true})
    {
        receive(0x3d); receive(0xc5); // An incomplete frame must be dropped later.
        if (tx_complete) send();
        USART1->DATAR.value = 0x3d; // Stale register contents, not a received byte.
        USART1->STATR |= early_error | (tx_complete ? USART_IT_TC : 0u);
        const unsigned reads = USART1->DATAR.reads;
        interrupt();
        CHECK(USART1->DATAR.reads == reads);
        CHECK((USART1->STATR & early_error) == early_error);
        CHECK(!(USART1->STATR & USART_FLAG_RXNE));
        CHECK(stopped()); // TC still releases DE without waiting for RXNE.

        // A later IRQ before RXNE must still leave the error and DATAR alone.
        interrupt();
        CHECK(USART1->DATAR.reads == reads);
        CHECK((USART1->STATR & early_error) == early_error);

        receive(0x3d); // RXNE arrives with the pending error; reject this sync byte.
        CHECK(USART1->DATAR.reads == reads + 1);
        CHECK(!(USART1->STATR & (15u | USART_FLAG_RXNE)));
        for (unsigned i = 1; i < sizeof(request); ++i) receive(request[i]);
        CHECK(bus_port_to_host.recv_data_len == 0);
        frame(); CHECK(bus_port_to_host.recv_data_len == 8);
        CHECK(memcmp(bus_port_to_host.bus_recv_data_ptr, request, 8) == 0);
        release();
    }

    // A damaged fast heartbeat must not survive in the parser's skip state.
    request[4] = 0x20;
    for (unsigned i = 0; i < 5; ++i) receive(request[i]);
    receive(0, USART_FLAG_FE);
    receive(0); receive(0); receive(0);
    CHECK(heartbeats == 0);
    frame(); CHECK(heartbeats == 1);
    request[4] = 0x21;

    // ORE must release its IRQ even with PE and no later byte to supply RXNE.
    // A ready byte needs no extra discard; an early PE may have lost its label.
    for (bool rx_ready : {false, true})
    for (bool tx_complete : {false, true})
    {
        frame(); // Keep a previously published frame intact through fault recovery.
        published = bus_port_to_host.bus_recv_data_ptr;
        receive(0x3d); receive(0xc5);
        if (tx_complete) send();
        USART1->DATAR.value = 0x3d;
        USART1->STATR |= USART_FLAG_ORE | USART_FLAG_PE |
                        (rx_ready ? USART_FLAG_RXNE : 0u) | (tx_complete ? USART_IT_TC : 0u);
        const unsigned reads = USART1->DATAR.reads;
        CHECK(service_pending_interrupts() == 1u);
        CHECK(!irq_pending()); // No further receive event is needed to resume the main loop.
        CHECK(USART1->DATAR.reads == reads + 1);
        CHECK(!(USART1->STATR & (15u | USART_FLAG_RXNE)));
        CHECK(stopped());
        CHECK(bus_port_to_host.bus_recv_data_ptr == published);
        CHECK(bus_port_to_host.recv_data_len == 8);
        CHECK(memcmp(published, request, 8) == 0);
        release();

        // Cleanup without a new byte, and a TX-complete IRQ, must not consume
        // the pending one-byte discard or leave an interrupt request asserted.
        USART1->STATR |= USART_FLAG_ORE;
        CHECK(service_pending_interrupts() == 1u);
        send(); USART1->STATR |= USART_IT_TC;
        CHECK(service_pending_interrupts() == 1u);
        CHECK(!irq_pending() && stopped());

        uint8_t heartbeat[sizeof(request)];
        memcpy(heartbeat, request, sizeof(request));
        heartbeat[4] = 0x20;
        const uint16_t crc = bus_crc16(heartbeat, sizeof(heartbeat) - 2u);
        heartbeat[6] = (uint8_t)crc;
        heartbeat[7] = (uint8_t)(crc >> 8);
        const unsigned before = heartbeats;
        for (uint8_t b : heartbeat) receive(b);
        // The unlabelled next byte must not start even a fast heartbeat frame.
        CHECK(heartbeats == before + (rx_ready ? 1u : 0u));
        CHECK(bus_port_to_host.recv_data_len == 0);
        // The discard is consumed once: the following ordinary frame survives.
        frame(); CHECK(bus_port_to_host.recv_data_len == 8);
        CHECK(memcmp(bus_port_to_host.bus_recv_data_ptr, request, 8) == 0);
        release();
    }

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
