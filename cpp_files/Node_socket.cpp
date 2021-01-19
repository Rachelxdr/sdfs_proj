#include "../header_files/Node_socket.h"

Node_socket::Node_socket() {
    //Nothing
    messages_received = queue<string>();
    queue_lock = PTHREAD_MUTEX_INITIALIZER;
}

int Node_socket::send_message(string msg, string ip_addr, MessageType type) {
    int s;
    int sock_fd;
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    // cout << "[DEBUG CLIENT SEND] target ip: " << ip_addr << endl;
    s = getaddrinfo(ip_addr.c_str(), PORT, &hints, &result);
    if (s) {
        fprintf(stderr, "[ERROR] getaddrinfo client: %s\n", gai_strerror(s));
        return 1;
    }

    struct addrinfo* util = result;
    while (util != NULL) {
        sock_fd = socket(util->ai_family, util->ai_socktype, util->ai_protocol);
        if (sock_fd < 0) {
            perror ("client socket error");
            util = util->ai_next;
            continue;
        }

        int optval = 1;
        int retval = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

        if (retval == -1) {
            perror("client setsocketopt error");
            return 1;
        }
        break;
    }

    if (util == NULL) {
        perror ("[ERROR] Failed to create client socket");
        return 1;
    }

    switch(type)
    {
        case INIT_REQ:
            msg = "INIT_REQ>>>" + msg;
            break;
        case INIT_RES:
            msg = "INIT_RES>>>" + msg;
            break;
        case HEARTBEAT:
            msg = "HEARTBEAT>>>" + msg;
            break;
        case SWITCH:
            msg = "SWITCH>>>" + msg;
            break;
        case JOIN_ERROR:
            msg = "JOIN_ERROR>>>" + msg;
            break;
        case PUT_REQ:
            msg = "PUT_REQ>>>" + msg;
            break;
        case ELECTION:
            msg = "ELECTION>>>" + msg;
            break;
        case NEW_MASTER:
            msg = "NEW_MASTER>>>" + msg;
            break;
        case ELECTION_RES:
            msg = "ELECTION_RES>>>" + msg;
            break;
        default:
            cout << "[ERROR] Invalid message type" <<endl;
            return 1;
    }
    // cout << "[DEBUG] Client socket created" <<endl;
    ssize_t bytes_sent;
    bytes_sent = sendto(sock_fd, msg.c_str(), strlen(msg.c_str()), 0, util->ai_addr, util->ai_addrlen);
    if(bytes_sent < 0) {
        perror("[ERROR] Client sends message");
        return 1;
    }
    
    // cout << "[DEBUG] Client message sent: " << msg <<" Target: " << ip_addr <<endl;
    
    freeaddrinfo(result);
    close(sock_fd);
    return 0;
}

int Node_socket::create_server_socket() {
    int s;
    int sock_fd;
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;
    
    s = getaddrinfo(NULL, PORT, &hints, &result);

    if (s) {
        fprintf(stderr, "[ERROR] getaddrinfo server: %s\n", gai_strerror(s));
        return 1;
    }

    struct addrinfo* util = result;
    while (util != NULL) {
        sock_fd = socket(util->ai_family, util->ai_socktype, util->ai_protocol);

        if (sock_fd < 0) {
            perror ("server socket error");
            util = util->ai_next;
            continue;
        }

        int optval = 1;
        int retval = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

        if (retval == -1) {
            perror("server setsocketopt error");
            return 1;
        }
        int b = ::bind(sock_fd, result->ai_addr, result->ai_addrlen);
        if (b != 0) {
            perror("server bind error");
            util = util->ai_next;
            continue;
        }
        break;

    }

    if (util == NULL) {
        cout << "[ERROR] Server failed to create socket" << endl;
        return 1;
    }

    // cout <<"[DEBUG] Server socket created" <<endl;
    char buf[MAX_MSG_SIZE];
    ssize_t bytes_received;
    struct sockaddr_storage addr;
    int addrlen = sizeof(addr);
    freeaddrinfo(result);
    while (1) {
        // cout << "[DEBUG] loop" <<endl;
        char buf[MAX_MSG_SIZE];
        ssize_t bytes_received;
        // While loop with recv??
        bytes_received = recvfrom(sock_fd, buf, sizeof(buf), 0, (struct sockaddr*)&addr, (socklen_t*)&addrlen);
        if (bytes_received == -1) {
            cout << "[ERROR] Server receives message" <<endl;
            return 1;
        }
        
        buf[bytes_received] = '\0';
        // printf("[DEBUG] server message received: %s\n", buf);
        fflush(stdout);
        pthread_mutex_lock(&queue_lock);
        // cout << "[DEBUG] mem addr of queue server: " << &messages_received <<endl;
        
        this->messages_received.push(buf);
        // cout << "[DEBUG] messages_received size in server: " << messages_received.size() <<endl;
        pthread_mutex_unlock(&queue_lock);
        bzero(buf, sizeof(buf));
    }
    // cout << "[DEBUG] bytes_received: "<< bytes_received <<endl;
    
    
    fflush(stdout);
    // buf[bytes_received] = '\0';
    // messages_received.push(buf);
    
    
    
    close(sock_fd);
    return 0;
}

// int Node_socket::create_tcp_server() {
//     int s;
//     int sock_fd;
//     struct addrinfo hints, *result;
//     memset(&hints, 0, sizeof(struct addrinfo));
//     hints.ai_family = AF_INET;
//     hints.ai_socktype = SOCK_STREAM;
//     hints.ai_flags = AI_PASSIVE;

//     s = getaddrinfo(NULL, TCP_PORT, &hints, &result);

//     if (s) {
//         fprintf(stderr, "getaddrinfo tco_server: %s\n", gai_strerror(s));
//         exit(1);
//     }

//     struct addrinfo* util = result;
//     while (util != NULL) {
//         sock_fd = socket(util->ai_family, util->ai_socktype, util->ai_protocol);

//         if (sock_fd < 0) {
//             perror("tcp server socket");
//             util = util->ai_next;
//             continue;
//         }
        
//         int optval = 1;
//         int retval = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

//         if (retval == -1) {
//             perror("tcp server setsockopt");
//             exit(1);
//         }

//         if (bind(sock_fd, result->ai_addr, result->ai_addrlen) != 0) {
//             perror("tcp server bind");
//             util = util->ai_next;
//             continue;
//         }

//         break;

//     }

//     if (util == NULL) {
//         perror("tcp server failed to bind");
//         exit(1);
//     }

//     if (listen(sock_fd, BACK_LOG) != 0) {
//         perror("tcp server listen");
//         exit(1);
//     }

//     while(1) {
//         socklen_t addr_size;
//         struct sockaddr_in incoming_info;
//         addr_size = sizeof(incoming_info);

//         int client_socket = accept(sock_fd, &incoming_info, &addr_size); // TODO: fix type error
//         // TODO handle received tcp messages.


//     }
// }