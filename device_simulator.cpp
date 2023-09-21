#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <sstream>
#include <string>
#include <vector>
#include <arpa/inet.h>

using std::vector;
using std::string;
using std::stringstream;

// Define the multicast address and port
const char* MULTICAST_ADDRESS = "224.3.11.15";
const int MULTICAST_PORT = 31115;

// Function to send a UDP response
void sendResponse(const std::string& response, const char* clientAddress, int clientPort) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(clientPort);
    inet_pton(AF_INET, clientAddress, &(serverAddr.sin_addr));

    sendto(sockfd, response.c_str(), response.length(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

    close(sockfd);
}

void replyResponse(int sockfd, const std::string& response, const char* clientAddress, int clientPort) {
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(clientPort);
    inet_pton(AF_INET, clientAddress, &(serverAddr.sin_addr));

    sendto(sockfd, response.c_str(), response.length(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
}

// Function to simulate a test and send status messages
void runTest(int sockfd, int duration, int rate, const char* clientAddress, int clientPort) {
    std::cout << "Starting test for " << duration << " seconds with rate " << rate << " ms." << std::endl;

    auto startTime = std::chrono::steady_clock::now();
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    while (true) {
        auto currentTime = std::chrono::steady_clock::now();
        int elapsedMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();

        if (elapsedMilliseconds >= duration * 1000) {
            replyResponse(sockfd, "STATUS;STATE=IDLE;", clientAddress, clientPort);
            break;
        }

        int mv = std::rand() % 1000; // Random millivolts
        int ma = std::rand() % 1000; // Random milliamps

        std::string statusMessage = "STATUS;TIME=" + std::to_string(elapsedMilliseconds) + ";MV=" + std::to_string(mv) + ";MA=" + std::to_string(ma) + ";";
        replyResponse(sockfd, statusMessage, clientAddress, clientPort);

        std::this_thread::sleep_for(std::chrono::milliseconds(rate));
    }
}

std::vector<string> parseString(const std::string& str, char delimiter) {
    stringstream input_stringstream(str);
    string parsed;
    vector<string> out;

    if (getline(input_stringstream, parsed, delimiter)) {
        out.push_back(parsed);
    }
    return out;
}

struct DurationRate {
    int seconds;
    int rateMS;
};

DurationRate geDurationRate(const string& str) {
    vector<string> comps = parseString(str, ';');
    if (comps.size() < 4) {
        return (DurationRate){-1,-1};
    }
    int duration = atoi(comps[2].c_str());
    int rateMS = atoi(comps[3].c_str());
    return (DurationRate) {duration, rateMS };
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <DeviceName> <Port>" << std::endl;
        return 1;
    }

    const char* deviceName = argv[1];
    int port = atoi(argv[2]);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error creating socket");
        return 1;
    }
    const int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("Error creating socket");
        return 1;
    }

    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Error binding socket");
        close(sockfd);
        return 1;
    }

    while (true) {
        char buffer[1024];
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);

        ssize_t receivedBytes = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (receivedBytes < 0) {
            perror("Error receiving data");
            close(sockfd);
            return 1;
        }

        buffer[receivedBytes] = '\0';
        std::string message(buffer);
        std::cout << "Received message: " << message << ", sending response to " << inet_ntoa(clientAddr.sin_addr) << ", port=" << ntohs(clientAddr.sin_port) << std::endl;

        if (message == "ID;") {
            // Respond to discovery message
            std::string response = "ID;MODEL=" + std::string(deviceName) + ";SERIAL=" + std::to_string(std::rand()) + ";";
            replyResponse(sockfd, response, inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
        } else if (message.find("TEST;CMD=START") != std::string::npos) {
            // Parse test parameters
            size_t durationPos = message.find("DURATION=");
            size_t ratePos = message.find("RATE=");

            std::cout << "durationPos="<<durationPos<<", ratePos"<<ratePos<<std::endl;
            if (durationPos != std::string::npos && ratePos != std::string::npos) {
                int duration = std::stoi(message.substr(durationPos + 9));
                int rate = std::stoi(message.substr(ratePos + 5));
                runTest(sockfd, duration, rate, inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
            } else {
                replyResponse(sockfd, "TEST;RESULT=error;MSG=Invalid parameters;", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
            }
        }
        // Other message types (e.g., STOP) can be handled similarly
    }

    close(sockfd);

    return 0;
}
