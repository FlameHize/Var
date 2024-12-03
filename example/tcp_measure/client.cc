#include "net/tcp/TcpClient.h"
#include "net/base/Logging.h"
#include "net/base/Thread.h"
#include "net/EventLoop.h"
#include "net/EventLoopThreadPool.h"
#include "metric/var.h"

using namespace var;
using namespace var::net;

#define LARGE_FILE_TRANSFER_BENCHMARK false

static int64_t kLargeFileSize = 1 * 1024 * 1024 * 1024;

class PingPongClient;

class Session : noncopyable {
public:
    Session(EventLoop* loop,
            const InetAddress& server_addr,
            const std::string& name,
            PingPongClient* owner) 
    : _client(loop, server_addr, name)
    , _owner(owner)
    , _total_bytes_read(0)
    , _total_messages_read(0)
    , _total_time_us(0)
    , _last_recv_time_us(gettimeofday_us()) {
        _client.setConnectionCallback(
            std::bind(&Session::onConnection, this, _1));
        _client.setMessageCallback(
            std::bind(&Session::onMessage, this, _1, _2, _3));
    }

    void start() {
        _client.connect();
    }

    void stop() {
        _client.disconnect();
    }

    int64_t bytesRead() const {
        return _total_bytes_read;
    }

    int64_t messagesRead() const {
        return _total_messages_read;
    }

    int64_t timeCost() const {
        return _total_time_us;
    }

private:
    void onConnection(const TcpConnectionPtr& conn);

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time) {
        ++_total_messages_read;
        _total_bytes_read += buf->readableBytes();
        
        int64_t cur_time = gettimeofday_us();
        int64_t latency = cur_time - _last_recv_time_us;
        _last_recv_time_us = cur_time;
        _total_time_us += latency;

#if LARGE_FILE_TRANSFER_BENCHMARK
        buf->retrieve(buf->readableBytes());
        if(_total_bytes_read == kLargeFileSize) {
            stop();
        }
#else 
        conn->send(buf);
#endif
    }


private:
    TcpClient _client;
    PingPongClient* _owner;
    int64_t _total_bytes_read;
    int64_t _total_messages_read;
    int64_t _total_time_us;
    int64_t _last_recv_time_us;
};

class PingPongClient : noncopyable {
public:
    PingPongClient(EventLoop* loop,
                   const InetAddress& server_addr,
                   int block_size,
                   int session_count,
                   int timeout,
                   int thread_count)
    : _loop(loop)
    , _thread_pool(loop, "pingpong-client")
    , _session_count(session_count)
    , _timeout(timeout)
    , _bytes_read_count(0)
    , _throughput("tcp_throughput_mb_s")
    , _latency("tcp_latency_us") {
#if LARGE_FILE_TRANSFER_BENCHMARK
#else
        _loop->runAfter(timeout, std::bind(&PingPongClient::handleTimeout, this));
        _loop->runEvery(1, std::bind(&PingPongClient::handleTimeInterval, this));
#endif
        if(thread_count > 1)  {
            _thread_pool.setThreadNum(thread_count);
        }
        _thread_pool.start();
        for(int64_t i = 0; i < block_size; ++i) {
            _message.push_back(static_cast<char>(i % 128));
        }
        for(int i = 0; i < session_count; ++i) {
            char buf[32];
            snprintf(buf, sizeof buf, "C%05d", i);
            Session* session = new Session(_thread_pool.getNextLoop(), server_addr, buf, this);
            session->start();
            _sessions.emplace_back(session);
        }
    }

    const std::string& message() const {
        return _message;
    }

    void onConnect() {
        if(_num_connected.incrementAndGet() == _session_count) {
            LOG_INFO << "all connected";
        }
    }

    void onDisconnect(const TcpConnectionPtr& conn) {
        if(_num_connected.decrementAndGet() == 0) {
            LOG_INFO << "all disconnected";
            int64_t total_bytes_read = 0;
            int64_t total_message_read = 0;
            int64_t total_time_cost = 0;
            int64_t average_latency = 0;
            for(const auto& session : _sessions) {
                total_bytes_read += session->bytesRead();
                total_message_read += session->messagesRead();
                total_time_cost += session->timeCost();
                average_latency += (session->timeCost() / session->messagesRead());
            }
            total_time_cost /= _sessions.size();
            average_latency /= _sessions.size();
            LOG_INFO << total_bytes_read << " total bytes read";
            LOG_INFO << total_message_read << " total messages read";
            LOG_INFO << static_cast<double>(total_bytes_read) / static_cast<double>(total_message_read)
                     << " average message size";
#if LARGE_FILE_TRANSFER_BENCHMARK
            LOG_INFO << static_cast<double>(total_time_cost / 1000) << "ms time cost";
            LOG_INFO << static_cast<double>(total_bytes_read) / (total_time_cost * 1024 * 1024 / 1000000)
                     << "MB/s throughput";
#else
            LOG_INFO << _timeout * 1000 << "ms time cost";
            LOG_INFO << static_cast<double>(total_bytes_read) / (_timeout * 1024 * 1024)
                     << " MB/s throughput";
#endif
            LOG_INFO << average_latency << "us average latency";
            conn->getLoop()->queueInLoop(std::bind(&PingPongClient::quit, this));
        }
    }

private:
    void quit() {
        _loop->queueInLoop(std::bind(&EventLoop::quit, _loop));
    }

    void handleTimeout() {
        LOG_INFO << "stop";
        for(auto& session : _sessions) {
            session->stop();
        }
    }

    void handleTimeInterval() {
        if(_num_connected.get() != _session_count) {
            return;
        }
        int64_t total_bytes_read = 0;
        int64_t average_latency = 0;
        for(const auto& session : _sessions) {
            total_bytes_read += session->bytesRead();
            average_latency += (session->timeCost() / session->messagesRead());
        }
        average_latency /= _sessions.size();
        int64_t throughput = (total_bytes_read - _bytes_read_count) / (1024 * 1024);
        _throughput << throughput;
        _bytes_read_count = total_bytes_read;

        _latency << average_latency;
    }

private:
    EventLoop* _loop;
    EventLoopThreadPool _thread_pool;
    int _session_count;
    int _timeout;
    std::vector<std::unique_ptr<Session>> _sessions;
    std::string _message;
    AtomicInt32 _num_connected;

    int64_t _bytes_read_count;
    LatencyRecorder _throughput;
    LatencyRecorder _latency;
};

void Session::onConnection(const TcpConnectionPtr& conn) {
    if(conn->connected()) {
        conn->setTcpNoDelay(true);
        conn->send(_owner->message());
        _owner->onConnect();
    }
    else {
        _owner->onDisconnect(conn);
    }
}

int main() {
    int64_t block_size = 0;
#if LARGE_FILE_TRANSFER_BENCHMARK
    block_size = kLargeFileSize;
#else 
    block_size = 16 * 1024;
#endif
    int session_count = 1;
    int timeout = 30;
    int thread_count = 8;

    StartDummyServerAt(8511);
    EventLoop loop;
    InetAddress addr("0.0.0.0", 1234);
    PingPongClient client(&loop, addr, block_size, session_count, timeout, thread_count);
    loop.loop();
}