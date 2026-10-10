#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hm2_eth.c"

static int send_result = -2, recv_result = -2;
static int send_calls, recv_calls, last_send_size;

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
    (void)board;
    (void)timeout;
    recv_calls++;
    int count = recv_result == -2 ? len : recv_result;
    if (count > 0) memset(buffer, 0x42, count < len ? count : len);
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
    comm_active = 1;
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
    reset(&board);
    for (unsigned i = 0; i < 62; i++)
        assert(hm2_eth_enqueue_read(&board.llio, 0x1000, data, 4) == 1);
    assert(hm2_eth_enqueue_read(&board.llio, 0x1000, data, 4) == 0);
    assert(board.queue_reads_count == 62);
    assert(hm2_eth_send_queued_reads(&board.llio) == 1);
    assert(board.queue_reads_count == 64);

    reset(&board);
    assert(hm2_eth_enqueue_read(&board.llio, 0x1000, data, 508) == 1);
    assert(hm2_eth_enqueue_read(&board.llio, 0x1000, data, 508) == 1);
    assert(hm2_eth_enqueue_read(&board.llio, 0x1000, data, 372) == 1);
    assert(hm2_eth_enqueue_read(&board.llio, 0x1000, data, 4) == 0);
    assert(board.queue_buff_size == 1388);
    assert(hm2_eth_send_queued_reads(&board.llio) == 1);
    assert(board.queue_buff_size == 1398);

    reset(&board);
    for (unsigned i = 0; i < 174; i++)
        assert(hm2_eth_enqueue_write(&board.llio, 0x1000, data, 4) == 1);
    assert(hm2_eth_enqueue_write(&board.llio, 0x1000, data, 4) == 0);
    assert(board.write_packet_ptr - board.write_packet == 1392);
    assert(hm2_eth_send_queued_writes(&board.llio) == 1);
    assert(last_send_size == 1400);
    assert(board.write_packet_ptr == board.write_packet);

    for (int count = -1; count < 20; count++) {
        reset(&board);
        assert(hm2_eth_enqueue_read(&board.llio, 0x1000, data, 4) == 1);
        send_result = count;
        assert(hm2_eth_send_queued_reads(&board.llio) == 0);
        assert(board.queue_buff_size == 0 && board.queue_reads_count == 0);
        assert(board.read_packet_ptr == board.read_packet);
    }
    for (int count = -1; count < 16; count++) {
        reset(&board);
        send_result = count;
        assert(hm2_eth_enqueue_write(&board.llio, 0x1000, data, 4) == 1);
        assert(hm2_eth_send_queued_writes(&board.llio) == 0);
        assert(board.write_packet_ptr == board.write_packet);
    }
    puts("PASS: transfer sizes -8..520, short/failed I/O, queue capacity, response bounds and trailer reservation");
    return 0;
}
