add_library(var_net SHARED 
    Acceptor.cc
    Buffer.cc
    Channel.cc
    Connector.cc
    EventLoop.cc
    EventLoopThread.cc
    EventLoopThreadPool.cc
    Socket.cc
    SocketsOps.cc
    InetAddress.cc
    Timer.cc
    TimerQueue.cc
    tcp/TcpConnection.cc
    tcp/TcpClient.cc
    tcp/TcpServer.cc
    udp/UdpServer.cc
    Poller.cc
    poller/PollPoller.cc
    poller/EpollPoller.cc
    poller/DefaultPoller.cc
    http/http_parser.cc
    http/http_method.cc
    http/http_status_code.cc
    http/http_url.cc
    http/http_header.cc
    http/http_message.cc
    http/http_server.cc
    base/AsyncLogging.cc
    base/Condition.cc
    base/CountDownLatch.cc
    base/CurrentThread.cc
    base/Date.cc
    base/Exception.cc
    base/FileUtil.cc
    base/LogFile.cc
    base/Logging.cc
    base/LogStream.cc
    base/ProcessInfo.cc
    base/Thread.cc
    base/Timestamp.cc
    base/TimeZone.cc
    base/StringPrintf.cc)   

target_include_directories(var_net PUBLIC ${PROJECT_SOURCE_DIR}/net)

add_subdirectory(test)
