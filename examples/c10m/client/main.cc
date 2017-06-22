#include <evpp/tcp_client.h>
#include <evpp/event_loop_thread_pool.h>
#include <evpp/buffer.h>
#include <evpp/tcp_conn.h>

#include <gflags/gflags.h>

DEFINE_string(localIpPort, "", "The local ip address and port to bind");
DEFINE_int32(localIpCount, 1, "The total count of local ip");
DEFINE_int32(connPerIp, 1, "The concurrent connection count for every local ip");
DEFINE_string(serverIpPort, "127.0.0.1:2007", "The tcp server ip address and listening port");
DEFINE_int32(messageLen, 26, "The length of the message sending to server");
DEFINE_int32(sleepIntervalMs, 1000, "The sleeping interval time between message sending on one connection");
DEFINE_int32(threadCount, 24, "The working thread count");


// 根据index计算当前ip的下一个IP
// 例如输入 "192.168.0.150:80", ipIndex=2 ===> "192.168.0.152:80"
std::string calcIpPort(const std::string& ipPort, int ipIndex) {
    std::vector<std::string> spp, dotip;
    evpp::StringSplit(ipPort, ":", 0, spp);
    evpp::StringSplit(spp[0], ".", 0, dotip);
    auto a = std::atoi(dotip[3].data());
    auto next = std::to_string(a + ipIndex);
    auto r = dotip[0] + "." + dotip[1] + "." + dotip[2] + "." + next + ":" + spp[1];
    return r;
}

class Client {
public:
    Client(evpp::EventLoop* loop)
        : loop_(loop){

        loop_->RunEvery(evpp::Duration(double(FLAGS_sleepIntervalMs) / 1000.0), 
                        std::bind(&Client::SendMessage, this));

        tpool_.reset(new evpp::EventLoopThreadPool(loop, FLAGS_threadCount));
        tpool_->Start(true);

        uint32_t headLen = htonl(FLAGS_messageLen);
        message_.append((char*)&headLen, sizeof(headLen));
        for (int i = 0; i < FLAGS_messageLen; ++i) {
            message_.push_back('a');
        }

        for (int ip_index = 0; ip_index < FLAGS_localIpCount; ip_index++) {
            for (int i = 0; i < FLAGS_connPerIp; i++) {
                auto local_addr = FLAGS_localIpPort;
                if (ip_index != 0) {
                    local_addr = calcIpPort(local_addr, ip_index);
                }

                std::shared_ptr<evpp::TCPClient> c(new evpp::TCPClient(tpool_->GetNextLoop(), FLAGS_serverIpPort, ""));
                c->SetConnectionCallback(
                    std::bind(&Client::OnConnection, this, std::placeholders::_1));
                c->SetMessageCallback(
                    std::bind(&Client::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
                c->set_connecting_timeout(evpp::Duration(100.0));
                c->Bind(local_addr);
                c->Connect();
                clients_.push_back(c);
            }
        }


    }

    ~Client() {}

    void OnConnection(const evpp::TCPConnPtr& conn) {
        if (conn->IsConnected()) {
            connected_count_++;
        } else {
            connected_count_--;
        }
    }

    void OnMessage(const evpp::TCPConnPtr& conn, evpp::Buffer* buf) {
        
    }


private:
    void SendMessage() {
        LOG_ERROR << "connected_count=" << connected_count_;
        for (auto client : clients_) {
            auto c = client->conn();
            if (c->IsConnected()) {
                c->Send(message_);
            }
        }
    }
private:
    evpp::EventLoop* loop_;
    std::shared_ptr<evpp::EventLoopThreadPool> tpool_;
    std::vector<std::shared_ptr<evpp::TCPClient>> clients_;
    std::atomic<int> connected_count_;
    std::string message_;
};

void UnitTest() {
    auto r = calcIpPort("192.168.0.150:80", 3);
    assert(r == "192.168.0.153:80");
}


int main(int argc, char* argv[]) {
    UnitTest();

    google::ParseCommandLineFlags(&argc, &argv, true);
    evpp::EventLoop loop;
    evpp::EventLoopThreadPool tpool(&loop, uint32_t(FLAGS_threadCount));
    tpool.Start(true);


    //Client client(&loop, serverAddr, blockSize, sessionCount, timeout, threadCount);
    loop.Run();
    return 0;
}


#ifdef WIN32
#include "../../winmain-inl.h"
#endif



