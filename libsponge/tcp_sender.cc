#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return this->_next_seqno - this->_acked_seqno; }

void TCPSender::fill_window() {
    if (this->_has_set_fin_flag) {
        return;
    }

    // 为防止死锁，发送数据的时候即便对面窗口为 0，也至少发送 1 单位的数据
    auto window_size = max<uint16_t>(1, this->_window_size);

    // 只要还有「预订要发送」但是这些数据还没填满 Receiver 窗口的空间，就从 _stream 中读取数据，然后发出去
    // 要读取到的数据需要充分考虑到 window_size，然后有选择性的读取数据的长度
    while (this->bytes_in_flight() < window_size) {
        TCPSegment segment_to_send;

        // 给 segment_to_send 添加 seqno
        segment_to_send.header().seqno = this->next_seqno();

        if (!this->_has_set_syn_flag) {
            segment_to_send.header().syn = true;
            this->_has_set_syn_flag = true;
        }

        // MAX_PAYLOAD_SIZE 只限制字符串长度并不包括 SYN 和 FIN，但是 window_size 包括 SYN 和 FIN
        auto payload_size = min(TCPConfig::MAX_PAYLOAD_SIZE,
                                min(window_size - this->bytes_in_flight() - segment_to_send.header().syn,
                                    this->_stream.buffer_size()));

        // 数据是从 _stream 中读取的
        auto payload = this->_stream.read(payload_size);
        segment_to_send.payload() = string(payload);

        // 如果读到 EOF 了且 window_size 还有空位，那就把 fin 给放进去
        // 注意这里有一个细节，那就是这里不需要检查 TCPConfig::MAX_PAYLOAD_SIZE，因为即便是它满了，那也可以放入 FIN
        if (!this->_has_set_fin_flag && _stream.eof() &&
            this->bytes_in_flight() + segment_to_send.length_in_sequence_space() < window_size) {
            segment_to_send.header().fin = true;
            this->_has_set_fin_flag = true;
        }

        // payload_true_size 包含了 fin 和 syn 后的大小
        uint64_t payload_true_size = segment_to_send.length_in_sequence_space();

        // 空数据报就不发送了
        if (payload_true_size == 0) {
            break;
        }

        // 发送数据，segments_out 中的数据被发送出去是调用者做的事情，Sender 无需关心
        this->segments_out().push(segment_to_send);

        // 如果定时器关闭，则启动定时器
        if (!this->_timer.is_running()) {
            this->_timer.start();
        }

        // 保存备份，重发时可能会用
        this->_outstanding_seg.emplace(this->_next_seqno, std::move(segment_to_send));

        // 更新序列号和发出但未 ACK 的字节数
        this->_next_seqno += payload_true_size;  // _next_seqno 是 absolute seqno
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // 和接收方原理一样，checkpoint 为 next_seqno_absolute
    auto abs_ackno = unwrap(ackno, _isn, next_seqno_absolute());

    // 传入的 ACK 是不可靠的，直接丢弃
    if (abs_ackno > this->next_seqno_absolute()) {
        return;
    }

    // 在接受到 ack 的时候如果收到窗口为 0，窗口大小发送方仍将其视为 1，允许发送 1 字节的数据。
    // 但是这个时候，未采用 TCP 标准的 指数退避策略（即 RTO 不翻倍）
    this->_window_size = window_size;

    // 用于标记是否有 segment 是否成功
    bool some_seg_ack_successful = false;

    // 处理已经收到的包（序列号空间要小于 ACK）
    while (!this->_outstanding_seg.empty()) {
        auto &[abs_seq, segment] = this->_outstanding_seg.front();
        if (abs_seq + segment.length_in_sequence_space() - 1 < abs_ackno) {
            some_seg_ack_successful = true;
            this->_acked_seqno += segment.length_in_sequence_space();
            this->_outstanding_seg.pop();
        } else {
            break;
        }
    }

    // 有成功 ACK 的包，则重置定时器（触发 RTO），清零连续重传次数
    if (some_seg_ack_successful) {
        this->_consecutive_retransmissions_count = 0;
        this->_timer.set_time_out(_initial_retransmission_timeout);
        this->_timer.start();
    }

    // 没有等待 ACK 的包了，则关闭定时器
    if (this->bytes_in_flight() == 0) {
        this->_timer.stop();
    }

    // 尝试继续填满窗口发送
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    // 把时间的自然流逝传递给 _timer
    this->_timer.tick(ms_since_last_tick);

    // 定时器超时（已经确保定时器已经打开），如果定时器关闭不会超时检查不会返回 true
    if (this->_timer.check_time_out() && !this->_outstanding_seg.empty()) {
        // 重传最早的报文
        this->_segments_out.push(_outstanding_seg.front().second);

        // window_size 非 0 对应的操作
        // 如果此时收到窗口为 0，则不触发 RTO 指数退避策略
        if (this->_window_size > 0) {
            ++this->_consecutive_retransmissions_count;
            this->_timer.set_time_out(this->_timer.get_time_out() * 2);
        }

        // 重启定时器
        this->_timer.start();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return this->_consecutive_retransmissions_count; }

void TCPSender::send_empty_segment() {
    // 发送空数据报，可以用于仅仅 ACK
    TCPSegment empty_segment;
    empty_segment.header().seqno = this->next_seqno();
    this->_segments_out.push(empty_segment);
}

bool TCPSender::all_data_ready() const { return this->stream_in().input_ended() && this->_outstanding_seg.empty(); }

bool TCPSender::has_sent_syn() const { return this->_has_set_syn_flag; }

bool TCPSender::has_sent_fin() const { return this->_has_set_fin_flag; }

size_t TCPSender::stream_buffer() const { return this->stream_in().buffer_size(); }
