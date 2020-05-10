////
//// Created by Anonymous275 on 3/3/2020.
////
#define ENET_IMPLEMENTATION

#include <condition_variable>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

int ClientID = -1;

extern int DEFAULT_PORT;
std::chrono::time_point<std::chrono::steady_clock> PingStart,PingEnd;
extern std::vector<std::string> GlobalInfo;
bool TCPTerminate = false;
bool Terminate = false;
bool CServer = true;
SOCKET*ClientSocket;
extern bool MPDEV;
int ping = 0;

void GameSend(const std::string&Data){
    if(!TCPTerminate) {
        int iSendResult = send(*ClientSocket, (Data + "\n").c_str(), int(Data.length()) + 1, 0);
        if (iSendResult == SOCKET_ERROR) {
            if (MPDEV)std::cout << "(Proxy) send failed with error: " << WSAGetLastError() << std::endl;
            TCPTerminate = true;
        } else {
            if (MPDEV && Data.length() > 1000) {
                std::cout << "(Launcher->Game) Bytes sent: " << iSendResult << std::endl;
            }
            //std::cout << "(Launcher->Game) Bytes sent: " << iSendResult <<  " : " << Data << std::endl;
        }
    }
}
void TCPSend(const std::string&Data);
void UDPSend(const std::string&Data);
void ServerSend(const std::string&Data, bool Rel){
    if(!Terminate){
        char C = 0;
        if(Data.length() > 3)C = Data.at(0);
        if (C == 'O' || C == 'T')Rel = true;

        if(Rel)TCPSend(Data);
        else UDPSend(Data);

        if (MPDEV && Data.length() > 1000) {
            std::cout << "(Launcher->Server) Bytes sent: " << Data.length()
            << " : "
            << Data.substr(0, 10)
            << Data.substr(Data.length() - 10) << std::endl;
        }else if(MPDEV && C == 'Z'){
            //std::cout << "(Game->Launcher) : " << Data << std::endl;
        }
    }
}
void NameRespond(){
    std::string Packet = "NR" + GlobalInfo.at(0)+":"+GlobalInfo.at(2);
    ServerSend(Packet,true);
}

void AutoPing(){
    while(!Terminate){
        ServerSend("p",false);
        PingStart = std::chrono::high_resolution_clock::now();
        std::this_thread::sleep_for(std::chrono::seconds (1));
    }
}

std::string UlStatus = "Ulstart";
std::string MStatus = " ";
void ServerParser(const std::string& Data){
    char Code = Data.at(0),SubCode = 0;
    if(Data.length() > 1)SubCode = Data.at(1);
    switch (Code) {
        case 'P':
            ClientID = std::stoi(Data.substr(1));
            break;
        case 'p':
            PingEnd = std::chrono::high_resolution_clock::now();
            ping = std::chrono::duration_cast<std::chrono::milliseconds>(PingEnd-PingStart).count();
            return;
        case 'N':
            if(SubCode == 'R')NameRespond();
            return;
        case 'M':
            MStatus = Data;
            UlStatus = "Uldone";
            return;
    }
    GameSend(Data);
}

void TCPClientMain(const std::string& IP,int Port);
void UDPClientMain(const std::string& IP,int Port);
void NetMain(const std::string& IP, int Port){
    std::thread Ping(AutoPing);
    Ping.detach();
    UDPClientMain(IP,Port);
    CServer = true;
    Terminate = true;
    std::cout << "Connection Terminated!" << std::endl;
}
extern SOCKET UDPSock;
extern SOCKET TCPSock;
void Reset() {
    ClientSocket = nullptr;
    TCPTerminate = false;
    Terminate = false;
    UlStatus = "Ulstart";
    MStatus = " ";
    UDPSock = -1;
    TCPSock = -1;
}

std::string Compress(const std::string&Data);
std::string Decompress(const std::string&Data);
void TCPGameServer(const std::string& IP, int Port){
    if(MPDEV)std::cout << "Game server Started! " << IP << ":" << Port << std::endl;
    do {
        Reset();
        if(CServer) {
            std::thread Client(TCPClientMain, IP, Port);
            Client.detach();
        }
        if(MPDEV)std::cout << "Game server on Start" << std::endl;
        WSADATA wsaData;
        int iResult;
        SOCKET ListenSocket = INVALID_SOCKET;
        SOCKET Socket = INVALID_SOCKET;

        struct addrinfo *result = nullptr;
        struct addrinfo hints{};
        char recvbuf[10000];
        int recvbuflen = 10000;

        // Initialize Winsock
        iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != 0) {
            if(MPDEV)std::cout << "(Proxy) WSAStartup failed with error: " << iResult << std::endl;
            exit(-1);
        }

        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE;
        // Resolve the server address and port
        iResult = getaddrinfo(nullptr, std::to_string(DEFAULT_PORT+1).c_str(), &hints, &result);
        if (iResult != 0) {
            if(MPDEV)std::cout << "(Proxy) getaddrinfo failed with error: " << iResult << std::endl;
            WSACleanup();
            break;
        }

        // Create a socket for connecting to server
        ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (ListenSocket == INVALID_SOCKET) {
            if(MPDEV)std::cout << "(Proxy) socket failed with error: " << WSAGetLastError() << std::endl;
            freeaddrinfo(result);
            WSACleanup();
            break;
        }

        // Setup the TCP listening socket
        iResult = bind(ListenSocket, result->ai_addr, (int) result->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            if(MPDEV)std::cout << "(Proxy) bind failed with error: " << WSAGetLastError() << std::endl;
            freeaddrinfo(result);
            closesocket(ListenSocket);
            WSACleanup();
            break;
        }

        freeaddrinfo(result);

        iResult = listen(ListenSocket, SOMAXCONN);
        if (iResult == SOCKET_ERROR) {
            if(MPDEV)std::cout << "(Proxy) listen failed with error: " << WSAGetLastError() << std::endl;
            closesocket(ListenSocket);
            WSACleanup();
            continue;
        }
        Socket = accept(ListenSocket, nullptr, nullptr);
        if (Socket == INVALID_SOCKET) {
            if(MPDEV)std::cout << "(Proxy) accept failed with error: " << WSAGetLastError() << std::endl;
            closesocket(ListenSocket);
            WSACleanup();
            continue;
        }
        closesocket(ListenSocket);
        if(MPDEV)std::cout << "(Proxy) Game Connected!" << std::endl;
        if(CServer){
            std::thread t1(NetMain, IP, Port);
            t1.detach();
            CServer = false;
        }

       ClientSocket = &Socket;
        do {
            //std::cout << "(Proxy) Waiting for Game Data..." << std::endl;
            iResult = recv(Socket, recvbuf, recvbuflen, 0);
            if (iResult > 0) {
                std::string buff;
                buff.resize(iResult*2);
                memcpy(&buff[0],recvbuf,iResult);
                buff.resize(iResult);

                ServerSend(buff,false);

            } else if (iResult == 0) {
                if(MPDEV)std::cout << "(Proxy) Connection closing...\n";
                closesocket(Socket);
                WSACleanup();
                Terminate = true;
                continue;
            } else {
                if(MPDEV)std::cout << "(Proxy) recv failed with error: " << WSAGetLastError() << std::endl;
                closesocket(Socket);
                WSACleanup();
                continue;
            }
        } while (iResult > 0);

        iResult = shutdown(Socket, SD_SEND);
        if (iResult == SOCKET_ERROR) {
            if(MPDEV)std::cout << "(Proxy) shutdown failed with error: " << WSAGetLastError() << std::endl;
            TCPTerminate = true;
            Terminate = true;
            closesocket(Socket);
            WSACleanup();
            continue;
        }
        closesocket(Socket);
        WSACleanup();
    }while (!TCPTerminate);
}

void VehicleNetworkStart();
void CoreNetworkThread();
void ProxyStart(){
    std::thread t1(CoreNetworkThread);
    if(MPDEV)std::cout << "Core Network Started!\n";
    t1.join();
}

void ProxyThread(const std::string& IP, int Port){
    auto*t1 = new std::thread(TCPGameServer,IP,Port);
    t1->detach();
    /*std::thread t2(VehicleNetworkStart);
    t2.detach();*/
}