#ifndef NODE_H
#define NODE_H

#include <iostream>
#include <tuple>
#include <string>
#include <map>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sstream>
#include <vector>
#include <pthread.h>
#include <functional>
#include <sys/stat.h>
#include <sys/types.h>

#include "./Node_socket.h"
#include "./Logger.h"
#include "./File_object.h"



// #define INACTIVE 0
#define ACTIVE 1
#define FAILED 2

// TODO: why?
#define T_fail 3
#define T_cleanup 3
#define T_period 5
#define GOSSIP_SIZE 4

#define HASH_MOD 128
#define ELECTION_TIMEOUT 4

#define MASTER_SELECTED 0
#define ELECTION_REQUIRED 1
#define WAITING_ELECTION_RES 2
#define WAITING_NEW_MASTER 3


using namespace std;

string get_ip();
string create_id();
vector<string> splitString(string s, string delim);
void* server_start(void* arg);
void* client_start(void* arg);
void* tcp_server_start(void* arg);

int get_hash_by_id(string node_id);
string get_ip_by_id(string node_id);



class Node {
    public:
        // tuple <string, string, string> id;
        string id; //ip_addr::port::join_time
        int hb;
        int local_clock;
        map <string, tuple <int, int, int>> membership_list; //id -> HB, clock, flag
        // map <string, tuple <int, int, int, int>> membership_list; //id -> HB, clock, flag, hash_id
        // Udp_socket* udp_util; // added tcp functions for mp2
        Node_socket* socket_util;
        int mode; // 0 = a2a, 1 = gossip
        Logger* node_logger;
        int status;
        int is_introducer;
        
        // MP2
        int is_master;
        int hash_id;
        vector <string> files_at_node;
        map <string, tuple <string, string, string, string>> master_file_table;
        map <string, string> master_node_table;
        string master_ip;
        int reelection_status; // 1 if master failed, 0 otherwise.
        map <int, string> hash_ring; // map node position on hash ring to node id by hashing the joining time% HASH_MOD
        //Master (introducer) does the hash
        int dir_set; // 1 if the sdfs directory is created. 0 otherwise.
        // vector<string> pending_elections;
        pthread_mutex_t thread_lock;

        // Constructor
        Node(int mode);


        void join_group();
        string pack_membership_list();
        void send_to_introducer(string msg);
        void read_message();
        void failure_detection();
        void send_hb();
        void update_self_info();
        void print_members_in_system();
        void print_membership_list();
        void send_switch_request();
        
        // MP2
        int create_sdfs_dir();
        void put_file(string local_name, string sdfs_name);
        // Master functions
        int check_hash_collision(string content);
        void leader_election();
        void broadcast(string msg, MessageType msg_type);

    private:
        void membership_list_init();
        void process_received_message(string type, string content);
        void merge_membership_list(string msg);
        void process_mem(string mem_id, string mem_info);
        vector<string> get_targets();
        void switch_mode(string target_mode);

        // MP2
        void update_hash_ring(int node_hash, string node_id);  
        void new_master_setup(string master_id);
        


};

#endif