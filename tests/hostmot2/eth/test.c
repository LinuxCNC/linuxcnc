#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hm2_eth.c"

static int send_result = -2, recv_result = -2;
static int send_calls, recv_calls, last_send_size;
static int queued_reply;

void rtapi_print_msg(msg_level_t level, const char *fmt, ...) {
    (void)level;
    (void)fmt;
}
void rtapi_print(const char *fmt, ...) { (void)fmt; }
int rtapi_task_self(void) { return -1; }
long long rtapi_get_time(void) { return 0; }

static int mock_send(hm2_eth_t *board, const void *buffer, int len) {
    (void)board;
    (void)buffer;
    assert(len > 0 && len <= HM2_ETH_PACKET_SIZE);
    last_send_size = len;
    send_calls++;
    return send_result == -2 ? len : send_result;
}
static int mock_recv(hm2_eth_t *board, void *buffer, int len, int timeout) {
    (void)timeout;
    recv_calls++;
    int count = recv_result == -2 ? len : recv_result;
    if (count > 0) memset(buffer, 0x42, count < len ? count : len);
    if (queued_reply && count == len) {
        assert(len >= 10);
        memcpy((char *)buffer + len - 8, &board->read_cnt, 4);
        memcpy((char *)buffer + len - 4, &board->write_cnt, 4);
    }
    return count;
}
static void reset(hm2_eth_t *board) {
    memset(board, 0, sizeof(*board));
    board->llio.private = board;
    board->read_packet_ptr = board->read_packet;
    board->write_packet_ptr = board->write_packet;
    board->eth_socket_send = mock_send;
    board->eth_socket_recv = mock_recv;
    send_result = recv_result = -2;
    send_calls = recv_calls = 0;
    queued_reply = 0;
    comm_active = 1;
}

/* 0: failed read send, 1: rejected read, 2: rejected write,
 * 3: rejected forced write followed by a successful flush,
 * 4/5: rejected write plus a failed read receive/send. */
static void test_failed_batch_recovery(unsigned mode) {
    hm2_eth_t board;
    reset(&board);
    typeof(*board.hal) hal = {0};
    rtapi_sint timeout = 100000, limit = 2, increment = 1, decrement = 1, level = 0;
    rtapi_uint error = 0, total = 0, exceeded = 0, io_error = 0;
    hal_bool_t io_error_pin = (hal_bool_t)&io_error;
    hal.read_timeout = (hal_sint_t)&timeout;
    hal.packet_error_limit = (hal_sint_t)&limit;
    hal.packet_error_increment = (hal_sint_t)&increment;
    hal.packet_error_decrement = (hal_sint_t)&decrement;
    hal.packet_error_level = (hal_sint_t)&level;
    hal.packet_error = (hal_bool_t)&error;
    hal.packet_error_total = (hal_uint_t)&total;
    hal.packet_error_exceeded = (hal_bool_t)&exceeded;
    board.hal = &hal;
    board.llio.io_error = &io_error_pin;
    rtapi_u32 data = 0x99999999;
    int read_sent = mode >= 2 && mode != 5;
    int read_received = read_sent && mode != 4;

    for (unsigned attempt = 0; attempt < 3; attempt++) {
        if (attempt == 2) hal_set_bool(io_error_pin, 0);
        if (mode >= 2) {
            int sends = send_calls;
            assert(hm2_eth_set_force_enqueue(&board.llio, 1) == 1);
            assert(hm2_eth_write(&board.llio, 0x1000, &data, 4) == 1);
            assert(hm2_eth_write(&board.llio, 0x1000, &data, 3) == 0);
            assert(hm2_eth_write(&board.llio, 0x1000, &data, 4) == 0);
            assert(hm2_eth_set_force_enqueue(&board.llio, 0) == 0);
            assert(send_calls == sends);
            if (mode == 3) {
                assert(hm2_eth_enqueue_write(&board.llio, 0x1000, &data, 4) == 1);
                assert(hm2_eth_send_queued_writes(&board.llio) == 1);
            }
        }
        assert(hm2_eth_enqueue_read(&board.llio, 0x1000, &data, 4) == 1);
        if (mode == 0 || mode == 5) send_result = -1;
        if (mode == 4) recv_result = -1;
        if (mode == 1) {
            assert(hm2_eth_enqueue_read(&board.llio, 0x1000, &data, 3) == 0);
            assert(hm2_eth_enqueue_read(&board.llio, 0x1000, &data, 4) == 0);
        }
        queued_reply = 1;
        assert(hm2_eth_send_queued_reads(&board.llio) == read_sent);
        assert(hm2_eth_receive_queued_reads(&board.llio) == (attempt == 1 ? 0 : read_received ? 1 : -EAGAIN));
        assert(data == (read_received ? 0x42424242 : 0x99999999));
        assert(recv_calls == (read_sent ? (int)attempt + 1 : 0));
        assert(board.llio.needs_soft_reset);
        assert(hal_get_bool(hal.packet_error));
        assert(hal_get_ui32(hal.packet_error_total) == attempt + 1);
        assert(hal_get_si32(hal.packet_error_level) == (attempt == 1 ? 2 : 1));
        assert(hal_get_bool(io_error_pin) == (attempt == 1));
        assert(hal_get_bool(hal.packet_error_exceeded) == (attempt == 1));
    }

    send_result = recv_result = -2;
    queued_reply = 1;
    assert(hm2_eth_enqueue_read(&board.llio, 0x1000, &data, 4) == 1);
    assert(hm2_eth_send_queued_reads(&board.llio) == 1);
    assert(hm2_eth_receive_queued_reads(&board.llio) == 1);
    assert(data == 0x42424242 && recv_calls == (read_sent ? 4 : 1));
    assert(board.queue_buff_size == 0 && board.queue_reads_count == 0);
    assert(!hal_get_bool(hal.packet_error) && !hal_get_bool(hal.packet_error_exceeded));
    assert(hal_get_si32(hal.packet_error_level) == 0);
    assert(hal_get_ui32(hal.packet_error_total) == 3);
    assert(!hal_get_bool(io_error_pin));
}

int main(void) {
    hm2_eth_t board;
    unsigned char data[1400] = {0};
    for (int size = -8; size <= 520; size++) {
        reset(&board);
        int valid = size == 0 || (size > 0 && size <= 508 && size % 4 == 0);
        assert(hm2_eth_read(&board.llio, 0x1000, data, size) == valid);
        assert(hm2_eth_write(&board.llio, 0x1000, data, size) == valid);
        assert(hm2_eth_enqueue_read(&board.llio, 0x1000, data, size) == valid);
        assert(hm2_eth_enqueue_write(&board.llio, 0x1000, data, size) == valid);
        if (!valid || size == 0) assert(send_calls == 0 && recv_calls == 0);
    }
    for (int count = -1; count < 8; count++) {
        reset(&board);
        memset(data, 0x99, sizeof(data));
        recv_result = count;
        assert(hm2_eth_read(&board.llio, 0x1000, data, 8) == 0);
        for (unsigned i = 0; i < sizeof(data); i++) assert(data[i] == 0x99);
    }
    for (int count = -1; count < 4; count++) {
        reset(&board);
        send_result = count;
        assert(hm2_eth_read(&board.llio, 0x1000, data, 8) == 0);
        assert(recv_calls == 0);
    }
    for (int count = -1; count < 12; count++) {
        reset(&board);
        send_result = count;
        assert(hm2_eth_write(&board.llio, 0x1000, data, 8) == 0);
    }
    for (int overflow = 0; overflow <= 1; overflow++) {
        reset(&board);
        for (unsigned i = 0; i < 62; i++)
            assert(hm2_eth_enqueue_read(&board.llio, 0x1000, data, 4) == 1);
        if (overflow) assert(hm2_eth_enqueue_read(&board.llio, 0x1000, data, 4) == 0);
        assert(hm2_eth_send_queued_reads(&board.llio) == !overflow);
        assert(board.queue_reads_count == (overflow ? 0 : 64));
        assert(send_calls == !overflow);

        reset(&board);
        assert(hm2_eth_enqueue_read(&board.llio, 0x1000, data, 508) == 1);
        assert(hm2_eth_enqueue_read(&board.llio, 0x1000, data, 508) == 1);
        assert(hm2_eth_enqueue_read(&board.llio, 0x1000, data, 372) == 1);
        if (overflow) assert(hm2_eth_enqueue_read(&board.llio, 0x1000, data, 4) == 0);
        assert(hm2_eth_send_queued_reads(&board.llio) == !overflow);
        assert(board.queue_buff_size == (overflow ? 0 : 1398));
        assert(send_calls == !overflow);

        reset(&board);
        for (unsigned i = 0; i < 174; i++)
            assert(hm2_eth_enqueue_write(&board.llio, 0x1000, data, 4) == 1);
        if (overflow) assert(hm2_eth_enqueue_write(&board.llio, 0x1000, data, 4) == 0);
        assert(hm2_eth_send_queued_writes(&board.llio) == !overflow);
        assert(send_calls == !overflow);
        if (!overflow) assert(last_send_size == 1400);
        assert(board.write_packet_ptr == board.write_packet);
        /* Initialization has no HAL error pins: report the dropped write once. */
        queued_reply = 1;
        for (unsigned retry = 0; retry < 2; retry++) {
            assert(hm2_eth_enqueue_read(&board.llio, 0x1000, data, 4) == 1);
            assert(hm2_eth_send_queued_reads(&board.llio) == 1);
            assert(hm2_eth_receive_queued_reads(&board.llio) == (!overflow || retry));
        }
    }

    for (int count = -1; count < 20; count++) {
        reset(&board);
        assert(hm2_eth_enqueue_read(&board.llio, 0x1000, data, 4) == 1);
        send_result = count;
        assert(hm2_eth_send_queued_reads(&board.llio) == 0);
        assert(board.queue_buff_size == 0 && board.queue_reads_count == 0);
        assert(board.read_packet_ptr == board.read_packet);
        for (recv_result = -1; recv_result <= 0; recv_result++) {
            memset(data, 0x99, sizeof(data));
            assert(hm2_eth_receive_queued_reads(&board.llio) == -EAGAIN);
            assert(recv_calls == 0);
            for (unsigned i = 0; i < sizeof(data); i++) assert(data[i] == 0x99);
        }
    }
    for (int count = -1; count < 16; count++) {
        reset(&board);
        send_result = count;
        assert(hm2_eth_enqueue_write(&board.llio, 0x1000, data, 4) == 1);
        assert(hm2_eth_send_queued_writes(&board.llio) == 0);
        assert(board.write_packet_ptr == board.write_packet);
    }
    puts("PASS: transfer sizes -8..520, short/failed I/O, queue capacity, response bounds and trailer reservation");
    for (unsigned mode = 0; mode < 6; mode++) test_failed_batch_recovery(mode);
    puts("PASS: failed/rejected batches, soft-error limit, user reset and successful queued retry");
    return 0;
}
