#include <iostream>
#include <string>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

using std::cout;
using std::string;

// Define the multicast address and port
const char* MULTICAST_ADDRESS = "224.3.11.15";
const int MULTICAST_PORT = 31115;
const int RECEIVE_BUFFER_SIZE = 1024;

// Function to send a UDP message
void sendResponse(const std::string& response, const char* clientAddress, int clientPort, int sockfd) {
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(clientPort);
    inet_pton(AF_INET, clientAddress, &(serverAddr.sin_addr));

    sendto(sockfd, response.c_str(), response.length(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
}

class RunTest {
private:
    int duration_;
    int rate_;
    sockaddr_in *clientAddr_;
    int sockfd_;
    bool running_;
    std::chrono::steady_clock::time_point startTime_ = std::chrono::steady_clock::now();
    int lastStatusTime_ = 0;

public:
    RunTest(int duration, int rate, struct sockaddr_in *clientAddr, int sockfd)
        : duration_(duration), rate_(rate), clientAddr_(clientAddr), sockfd_(sockfd), running_(true) {
            std::cout << "Starting test: duration=" << duration << "s, rate=" << rate << "ms" << std::endl;
        }

    void checkProgress() {
        auto currentTime = std::chrono::steady_clock::now();
        int elapsedMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime_).count();
        std::cout << "elapsedMilliseconds="<<elapsedMilliseconds<<std::endl;

        if (elapsedMilliseconds >= duration_ * 1000) {
            std::string statusMessage = "STATUS;STATE=IDLE;";
            std::cout << "RunTest: sending " << statusMessage << std::endl;
            sendResponse(statusMessage, inet_ntoa(clientAddr_->sin_addr), ntohs(clientAddr_->sin_port), sockfd_);
            running_ = false;
            return;
        }

        if (elapsedMilliseconds - lastStatusTime_ >= rate_) {
            int mv = std::rand() % 1000; // Random millivolts
            int ma = std::rand() % 1000; // Random milliamps

            std::string statusMessage = "STATUS;TIME=" + std::to_string(elapsedMilliseconds) + ";MV=" + std::to_string(mv) + ";MA=" + std::to_string(ma) + ";";
            std::cout << "RunTest: sending " << statusMessage << std::endl;
            sendResponse(statusMessage, inet_ntoa(clientAddr_->sin_addr), ntohs(clientAddr_->sin_port), sockfd_);
            lastStatusTime_ = elapsedMilliseconds;
        }
    }

    bool isRunning() const {
        return running_;
    }
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <DeviceName> <Port> <MulticastPort>" << std::endl;
        return 1;
    }

    const char* deviceName = argv[1];
    int listenPort = atoi(argv[2]);
    int multicastPort = atoi(argv[3]);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error creating multicast socket");
        return 1;
    }
    // Allow multiple sockets to bind to the same multicast address
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("Error setting multicast socket reuse");
        close(sockfd);
        return 1;
    }
    // Bind the socket to the multicast address
    sockaddr_in multicastAddr;
    memset(&multicastAddr, 0, sizeof(multicastAddr));
    multicastAddr.sin_family = AF_INET;
    multicastAddr.sin_addr.s_addr = inet_addr(MULTICAST_ADDRESS);
    multicastAddr.sin_port = htons(multicastPort);
    if (bind(sockfd, (struct sockaddr*)&multicastAddr, sizeof(multicastAddr)) < 0) {
        perror("Error binding socket to multicast address");
        close(sockfd);
        return 1;
    }
    // Join the multicast group
    ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_ADDRESS);
    mreq.imr_interface.s_addr = INADDR_ANY;
    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("Error joining multicast group");
        close(sockfd);
        return 1;
    }

    /*
     * Create a socket (endpoint) for this simulated device
     */
    int dev_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (dev_sockfd < 0) {
        perror("Error creating device socket");
        return 1;
    }
    int max_fd = std::max(sockfd, dev_sockfd);
    cout << "sockfd=" << sockfd << ", dev_sockfd=" << dev_sockfd << ", max_fd=" << max_fd << ", listenPort=" << listenPort << std::endl;

    fd_set readfds;
    struct timeval timeout;
    char buffer[RECEIVE_BUFFER_SIZE];
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    std::shared_ptr<RunTest> runTest;

    while (true) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(dev_sockfd, &readfds);

        timeout.tv_sec = 1; // Wait for up to 1 second
        timeout.tv_usec = 0;

        int selectResult = select(max_fd + 1, &readfds, nullptr, nullptr, &timeout);

        if (selectResult < 0) {
            perror("Error in select");
            break;
        } else if (selectResult == 0) {
            // No data received during the timeout
            if (runTest && runTest->isRunning()) {
                runTest->checkProgress();
            }
            continue;
        }

        if (FD_ISSET(sockfd, &readfds)) {
            ssize_t receivedBytes = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&clientAddr, &clientAddrLen);
            if (receivedBytes < 0) {
                perror("Error receiving data");
                break;
            }
            
            buffer[receivedBytes] = '\0';
            std::string message(buffer);
            std::cout << "Received message: " << message << std::endl;

            if (message == "ID;") {
                // Respond to discovery message
                std::string response = "ID;MODEL=" + std::string(deviceName) + ";SERIAL=" + std::to_string(std::rand()) + ";";
                sendResponse(response, inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), dev_sockfd);
            } else if (message.find("TEST;CMD=START") != std::string::npos) {
                // Parse test parameters
                if (runTest && runTest->isRunning()) {
                    std::cout << "Error: test already running" << std::endl;
                    continue;
                }
                size_t durationPos = message.find("DURATION=");
                size_t ratePos = message.find("RATE=");
                std::cout << "Running test durationPos=" << durationPos << std::endl;

                if (durationPos != std::string::npos && ratePos != std::string::npos) {
                    int duration = std::stoi(message.substr(durationPos + 9));
                    int rate = std::stoi(message.substr(ratePos + 5));
                    runTest.reset(new RunTest(duration, rate, &clientAddr, dev_sockfd));
                } else {
                    sendResponse("TEST;RESULT=error;MSG=Invalid parameters;", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), dev_sockfd);
                }
            }
            // Other message types (e.g., STOP) can be handled similarly
        }
    }

    close(sockfd);
    close(dev_sockfd);

    return 0;
}
