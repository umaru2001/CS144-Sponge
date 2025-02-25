#ifndef SPONGE_LIBSPONGE_TCP_SENDER_TIMER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_TIMER_HH

#include <cstdint>

class TCPSenderTimer {
  private:
    uint32_t _time_count = 0;

    uint32_t _time_out = 0;

    bool _is_running = false;

  public:
    TCPSenderTimer() = default;

    TCPSenderTimer(const uint32_t time_out) : _time_out(time_out) {}

    // 启动计时器
    void start() { _is_running = true; _time_count = 0; }

    // 停止计时器
    void stop() { _is_running = false; }

    // 设置计时器的超时时间
    void set_time_out(const uint32_t time_out) { _time_out = time_out; }

    // 获取当前设置的超时时间
    uint32_t get_time_out() const { return _time_out; }

    // 更新计时器的时间计数
    void tick(const std::size_t ms_since_last_tick) {
        if (_is_running)
            _time_count += ms_since_last_tick;
    }

    // 检查计时器是否超时
    bool check_time_out() const { return _is_running && _time_count >= _time_out; }

    // 检查计时器是否正在运行
    bool is_running() const { return _is_running; }
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_TIMER_HH
