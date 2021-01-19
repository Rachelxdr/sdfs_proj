#include "../header_files/Node.h"
#include "./util.cpp"

Node::Node(int hb_mode) {
   id = create_id();
   hb = 0;
   local_clock = 0;
   mode = hb_mode;
   node_logger = new Logger(id + "_log.txt");
   status = FAILED;
   is_master = 0;
   socket_util = new Node_socket();
   reelection_status = MASTER_SELECTED;
   master_ip = INTRODUCER_IP;
   dir_set = 0;
   thread_lock = PTHREAD_MUTEX_INITIALIZER;
//    hash_id = generate_hash_id();
}

int Node::create_sdfs_dir() {
    // dir name: sdfs_id
    string path_name = this->id + "_sdfs";
    int retval = mkdir(path_name.c_str(), 0777);
    if (retval != 0) {
        perror("Create sdfs directory: ");
        return 1;
    }
    this->dir_set = 1;
    return 0;
}

void Node::join_group() {
    this->status = ACTIVE;
    this->membership_list_init();

    if (this->files_at_node.size() != 0) {
        this->files_at_node.clear();
    }

    pthread_t client_thread;
    int client_create = pthread_create(&client_thread, NULL, client_start, (void*)this);
}

void Node::process_received_message(string type, string content) {
    if (type == "INIT_REQ") {
        // Current node is the introducer
        // New node joining
        // Send membership list over
        // content ip::port::start_time==hb#flag$$hash_id

        //May cause some nodes never get to join the group???
        vector<string> parse_result = splitString(content, "::");
        string response_target_ip = parse_result[0];
        int check_collision_result = check_hash_collision(content); // 1 if there is collision, 0 otherwise
        if (check_collision_result == 1) {
            //TODO send error message;
            string msg = "Unable to join group due to hash collision";
            int send_ret = this->socket_util->send_message(msg, response_target_ip, JOIN_ERROR);
            if (send_ret != 0) {
                cout << "[ERROR] Message not sent" << endl;
            }
        } else {
            string membership_list_msg = pack_membership_list();
            int send_ret = this->socket_util->send_message(membership_list_msg, response_target_ip, INIT_RES);
            if (send_ret != 0) {
                cout << "[ERROR] Message not sent" << endl;
            }

        }
        
    } else if (type == "INIT_RES") {
        pthread_mutex_lock(&thread_lock);
        if (this->dir_set == 0) {
            int create_ret = this->create_sdfs_dir();
            if (create_ret != 0) {
                cout <<"[ERROR] Failed to create sdfs directory" << endl;
            }
        }
        
        pthread_mutex_unlock(&thread_lock);
        update_hash_ring(this->hash_id, this->id);
        merge_membership_list(content);
    } else if (type == "HEARTBEAT") {
        merge_membership_list(content);
    } else if (type == "SWITCH") {
        switch_mode(content);
    } else if (type == "JOIN_ERROR") {
        // TODO 
        this->status = FAILED;
        string msg = "[JOIN_FAILED] Hash Collision";
        this->node_logger->log(msg);
        cout << msg << endl;
    } else if (type == "ELECTION") {
        /*
            1. Response with ELECTION_RES
            2. Start node election round
        */
       string target_ip = get_ip_by_id(content);
    //    string hash_str = to_string(this->hash_id);
       cout <<"[ELECTION RECEIVED] from node ip_addr: " << target_ip << "node hash_id: " << to_string(get_hash_by_id(content)) <<endl;
       this->socket_util->send_message(this->id, target_ip, ELECTION_RES);
       cout << "[ELECTION_RES SEND] My hash_id: "<< to_string(get_hash_by_id(this->id)) << endl;
       this->reelection_status = ELECTION_REQUIRED;
    } else if (type == "ELECTION_RES") {
        // Remove the ip of the sending node from the pending elections message
        // Wait for a MEW_MASTER message with in a time out (ELECTION_TIMEOUT?)
        string target_ip = get_ip_by_id(content);
        cout <<"[ELECTION_RES RECEIVED] from node ip_addr: " + target_ip + "node hash_id: " + to_string(get_hash_by_id(content)) <<endl;
        this->reelection_status = WAITING_NEW_MASTER;
        sleep(ELECTION_TIMEOUT);
        if (this->reelection_status == WAITING_NEW_MASTER) {
            this->reelection_status = ELECTION_REQUIRED;
        }

    } else if (type == "NEW_MASTER") {
        this->master_ip = get_ip_by_id(content);
        string msg = "[NEW MASTER] Received new master information from node: " + content;
        this->node_logger->log(msg);
        cout << msg << endl;
        this->new_master_setup(content);
    }
}

void Node::new_master_setup(string master_id) {
    this->reelection_status= MASTER_SELECTED;
    if (master_id == this->id) {
        this->is_master = 1;
    }
    this->master_ip = get_ip_by_id(master_id);
    string msg = "[MASTER SETUP] Set up new master with ip address: " + master_ip;
        this->node_logger->log(msg);
}

void Node::update_hash_ring(int node_hash, string node_id) {
    this->hash_ring.insert(pair<int, string> (node_hash, node_id));
}

void Node::print_membership_list() {
    cout << "========================================= MEMBERSHIP LIST INFO =========================================" << endl;
    cout << "               Member ID                 Heartbeat Counter             Clock                Status      " << endl;
        //    172.17.0.2::9000::140565190499201              44                      1                  active
    map <string, tuple<int, int, int>> :: iterator it;
    for (it = this->membership_list.begin(); it != this->membership_list.end(); it++) {
            string mem_id = it->first;
            tuple<int, int, int> mem_info = it->second;
            string mem_hb = to_string(get<0>(mem_info));
            string mem_clock = to_string(get<1>(mem_info));
            int flag = get<1>(mem_info);
            string status; 
            if (flag == FAILED) {
                status = "Inactive";
            } else {
                status = "Active";
            }
            cout << "  " << mem_id << "  " << "           " << mem_hb << "                       " << mem_clock << "                   " << status << "         "<<endl;

    }
    cout << "========================================================================================================" << endl;
}

void Node::print_members_in_system() {
    cout << "=============== Current Members ================"<<endl;
    int ct = 1;
    map <string, tuple<int, int, int>> :: iterator it;
    for (it = this->membership_list.begin(); it != this->membership_list.end(); it++) {
        cout << "[" << ct << "] " << it->first << endl;
        ct++;
    }
    cout << "================================================="<<endl;
}

string Node::pack_membership_list() {
    string msg;
    map <string, tuple<int, int, int>> :: iterator it;
    for (it = this->membership_list.begin(); it != this->membership_list.end(); it++) {
        string member_id = it->first;
        tuple <int, int, int> member_info = it->second;
        string member_hb = to_string(get<0>(member_info));
        string member_flag = to_string(get<2>(member_info));

        // ip_addr::port::join_time==hb#flag||ip_addr::port...
        msg += member_id + "==" + member_hb + "#" + member_flag + "||";
    }
    string sub = msg.substr(msg.length() - 2, 2);
    if (sub.compare("||")) {
        return "ERROR";
    }
    msg.erase(msg.length() - 2, 2);
    return msg;
}

void Node::update_self_info() {
    tuple <int, int, int> new_info = make_tuple(this->hb, this->local_clock, this->status);
    this->membership_list[this->id] = new_info;
}

void Node::membership_list_init() {
    if (this->membership_list.size() != 0) {
        this->membership_list.clear();
    }
    tuple <int, int, int> info = make_tuple(this->hb, this->local_clock, this->status);
    this->membership_list.insert(pair<string, tuple<int, int, int>>(this->id, info));
}

void Node::send_to_introducer(string msg) {
    int send_ret = this->socket_util->send_message(msg, INTRODUCER_IP, INIT_REQ);
    if (send_ret != 0) {
        cout << "[ERROR] Node to introdocer message not sent" << endl;
    }
}

int Node::check_hash_collision(string content) {
    // ip_addr::port::join_time==hb#flag$$hash_id
    vector<string> get_hash = splitString(content, "$$");
    int new_hash = stoi(get_hash[1]);

    map<int, string>::iterator it;
    it = this->hash_ring.find(new_hash);
    if (it != this->hash_ring.end()) {
        return 1;
    }
    return 0;

}

void Node::send_hb() {
    string msg_to_send = pack_membership_list();
    // map<string, tuple<int, int, int>>::iterator it;
    // vector <string> all_targets;
    // for (it = this->membership_list.begin(); it != this->membership_list.end(); it++) {
    //     if (it->first != this->id && get<2>(it->second) != FAILED) {
    //         vector<string> parse_id = splitString(it->first,  "::");
    //         if (parse_id.size() != 3) {
    //             cout << "[ERROR] Incorrect ip format in membership list" << endl;
    //         }
    //         string target_ip = parse_id[0];
            
    //     }
    // }
    vector <string> targets_to_send = this->get_targets();
    // if (this->mode == 1 && all_targets.size() > GOSSIP_SIZE) {
    //     for (int k = 0; k < GOSSIP_SIZE; k++) {
    //         int rand_num = rand() % all_targets.size();
    //         targets_to_send.push_back(all_targets[rand_num]);
    //         all_targets.erase(all_targets.begin() + rand_num);
    //     }
    // } else {
    //     targets_to_send = all_targets;
    // }
    for (int i = 0; i < targets_to_send.size(); i++) {
        this->socket_util->send_message(msg_to_send, targets_to_send[i], HEARTBEAT);
    }
}

vector<string> Node::get_targets() {
    vector <string> all_targets;
    map<string, tuple<int, int, int>>::iterator it;
    for (it = this->membership_list.begin(); it != this->membership_list.end(); it++) {
        if (it->first != this->id && get<2>(it->second) != FAILED) {
            vector<string> parse_id = splitString(it->first,  "::");
            if (parse_id.size() != 3) {
                cout << "[ERROR] Incorrect ip format in membership list" << endl;
            }
            string target_ip = parse_id[0];
            all_targets.push_back(target_ip);
        }
    }
    vector <string> targets_to_send;
    if (this->mode == 1 && all_targets.size() > GOSSIP_SIZE) {
        for (int k = 0; k < GOSSIP_SIZE; k++) {
            int rand_num = rand() % all_targets.size();
            targets_to_send.push_back(all_targets[rand_num]);
            cout << "[INFO] Gossip target: " << all_targets[rand_num] << endl;;
            all_targets.erase(all_targets.begin() + rand_num);
        }
    } else {
        targets_to_send = all_targets;
    }
    return targets_to_send;
}

void Node::send_switch_request() {
    string target_mode;
    if (this->mode == 0) {
        target_mode = "Gossip";
    } else {
        target_mode = "All2All";
    }
    map<string, tuple<int, int, int>>::iterator it;
    for (it = this->membership_list.begin(); it != this->membership_list.end(); it++) {
        if (get<2>(it->second) != FAILED) {
            vector<string> parse_id = splitString(it->first,  "::");
            if (parse_id.size() != 3) {
                cout << "[ERROR] Incorrect ip format in membership list" << endl;
            }
            string target_ip = parse_id[0];
            this->socket_util->send_message(target_mode, target_ip, SWITCH);
        }
    }
}

void Node::switch_mode(string target_mode) {
    if (target_mode == "Gossip" && this->mode == 0) {
        this->mode = 1;
        string msg = "[SWITCH] Switched mode to GOSSIP";
        this->node_logger->log(msg);
        cout << msg << endl;
        return;
    }

    if (target_mode == "All2All" && this->mode == 1) {
        this->mode = 0;
        string msg = "[SWITCH] Switched mode to All2All";
        this->node_logger->log(msg);
        cout << msg << endl;
        return;
    }
    
}

void Node::merge_membership_list(string msg) {
    // ip_addr::port::join_time==hb#flag||ip_addr::port...
    // cout << "[DEBUG] membership message before split string: "<< msg << endl;
    vector<string> members = splitString(msg, "||");
    // cout << "[DEBUG] =================== member vector ====================== " << endl;
    // int ct = 0;
    // for (string cur_mem : members) {
    //     cout << "At position: " << ct << " " << cur_mem << endl;
    //     ct++;
    // }
    for (int i = 0; i < members.size(); i++) {
        // cout << "[DEBUG MEMBER] Received member info: " << members[i] <<endl;
        vector<string> parse_mem = splitString(members[i], "==");
            if (parse_mem.size() != 2) {
                cout << "[ERROR] HEARTBEAT format invalid" <<endl;
                continue;
            }
            string mem_id = parse_mem[0]; // ip_addr::port::join_time
            string mem_info = parse_mem[1]; // hb# flag
            vector<string> parse_info = splitString(mem_info, "#");
            if (parse_info.size() != 2) {
                cout << "[ERROR] Member info format error" <<endl;
                continue;
            }

            int mem_hb = stoi(parse_info[0]);
            int mem_flag = stoi(parse_info[1]);

            /*
            1. If member is self:
                a. If marked as failed, change this->Status to FAILED and break
                b. Do nothing ?
            2. <NOT SELF> If id doesn't exist in the list, add new member
            3. <NOT SELF> <NOT NEW> If marked as failed, change the member flag on list and update local time
            4. <NOT SELF> <NOT NEW> <NOT FAILED> If incoming hb is larger than local hb, update hb and local time.
            5. <NOT SELF> <NOT NEW> <NOT FAILED> <SMALLER/EQ HB VAL> Do nothing. 
            */
        
           if (mem_id == this->id) { // Member is self
               if (mem_flag == FAILED) {
                   this->status = FAILED;
                   string log_msg = "[VOLUNTARILY LEAVE] " + this->id + " leaves group";
                   this->node_logger->log(log_msg); 
                   break;
               }
           } else {
               // TODO: other cases
                map<string, tuple<int, int, int>>::iterator it;
                it = this->membership_list.find(mem_id);
                if (it == this->membership_list.end()) {
                    if (mem_flag != FAILED) { // New member
                        // Add new member information to memberhship list
                        tuple <int, int, int> mem_to_list = make_tuple(mem_hb, this->local_clock, mem_flag);
                        this->membership_list.insert(pair<string, tuple<int, int, int>>(mem_id, mem_to_list));

                        // Add new member information to hash_ring
                        int new_member_hash = get_hash_by_id(mem_id);
                        this->update_hash_ring(new_member_hash, mem_id);


                        // Log information
                        string log_msg = "[NEW MEMBER] " + mem_id;
                        cout << log_msg << endl;
                        this->node_logger->log(log_msg);
                    }
                } else {
                    int hb_in_list = get<0>(this->membership_list[mem_id]);
                    int clock_in_list = get<1>(this->membership_list[mem_id]);
                    int flag_in_list = get<2>(this->membership_list[mem_id]);

                    if (mem_flag == FAILED && flag_in_list != FAILED) { // Received as failed member
                        tuple<int, int, int> new_tuple = make_tuple(hb_in_list, this->local_clock, FAILED);
                        this->membership_list[mem_id] = new_tuple;
                        string log_msg = "[FAILURE INFO] " + mem_id;
                        cout << log_msg << endl;
                        this->node_logger->log(log_msg);
                    } else{
                        if (hb_in_list < mem_hb) { // Higher hb value
                            tuple<int, int, int> new_tuple = make_tuple(mem_hb,  this->local_clock, mem_flag);
                            this->membership_list[mem_id] = new_tuple;
                        } 
                    }
                }
           }
            

    }
}

void Node::failure_detection() {
    map<string, tuple<int, int, int>>::iterator it;
    vector<string> remove_members;
    for (it = this->membership_list.begin(); it != this->membership_list.end(); it++) {
        string mem_id = it->first;
        int mem_hb = get<0>(it->second);
        int mem_clock = get<1>(it->second);
        int mem_flag = get<2>(it->second);
         /*
         !! Skip self
         1. if member is failed and local_clock - mem_clock is more than T_cleanup: Add to remove member.
         2. <NOT FAILED> if local_clock - mem_clock is more than T_fail: mark member as failed
         */
        if (mem_id == this->id) {
            continue;
        } else {
            if (mem_flag == FAILED && this->local_clock - mem_clock > T_cleanup) {
                remove_members.push_back(mem_id);
            } else {
                if (mem_flag != FAILED && this->local_clock - mem_clock > T_fail) {
                    tuple<int, int, int> new_tuple = make_tuple(mem_hb, mem_clock, FAILED);
                    this->membership_list[mem_id] = new_tuple;

                    // If failed member is the leader, set reelection_status flag to 1
                    vector<string> parse_mem_id = splitString(mem_id, "::");
                    if (parse_mem_id[0] == master_ip) {
                        string msg = "[ELECTION] Old master: " + master_ip + " detected at failed";
                        this->node_logger->log(msg);
                        cout << msg << endl;
                        this->reelection_status = ELECTION_REQUIRED;
                    }

                    string log_msg = "[FAILURE DETECTION] " + mem_id + "current clock: " + to_string(this->local_clock) + " member clock: " + to_string(mem_clock) + "member hb: " + to_string(mem_hb);
                    this->node_logger->log(log_msg);
                    cout << log_msg << endl;
                }
            }
        }
        
    }
    for (int i = 0; i < remove_members.size(); i++ ) {
        // Remove from membership list
        this->membership_list.erase(remove_members[i]);

        //Remove from hash ring
        int remove_hash = get_hash_by_id(remove_members[i]);
        this->hash_ring.erase(remove_hash);

        // Log informatin
        string log_msg = "[REMOVE] " + remove_members[i];
        cout << log_msg <<endl;
        this->node_logger->log(log_msg);
    }
}

void Node::read_message(){
    pthread_mutex_lock(&this->socket_util->queue_lock);

    
    // cout << "[DEBUG] mem addr of queue update_list: " << &this->socket_util->messages_received<<endl;
   
    queue<string> messages_to_process(this->socket_util->messages_received);
    this->socket_util->messages_received = queue<string>();
    
    pthread_mutex_unlock(&this->socket_util->queue_lock);
    // Message format : MessageType>>>message
    string msg;
    // cout << "[DEBUG] updating membership list" <<endl;
    // cout << "[DEBUG] messages_to_process size: " << messages_to_process.size() <<endl;
    while (!messages_to_process.empty()) {
        
        msg = messages_to_process.front();
        // cout << "[DEBUG] Node msg from queue msg: " << msg << endl; 
        // Parse message type
        vector<string> str_v = splitString(msg, ">>>");
        // for (int i = 0; i < str_v.size(); i++) {
        //     cout<< "[DEBUG CLIENT] str_v at " << i << " : " << str_v[i] <<endl;
        //     fflush(stdout);
        // }
        if (str_v.size() == 2) {
            string type = str_v[0];
            string content = str_v[1];
            process_received_message(type, content);
        } else {
            cout << "[ERROR] Invalid queued message format "<<endl;
        }
        messages_to_process.pop();
    }
    // sleep(10);
}

void Node::broadcast(string msg, MessageType msg_type) {
    map<string, tuple<int, int, int>>::iterator it;
    for (it = this->membership_list.begin(); it != this->membership_list.end(); it++) {
        string target_node_id = it->first;
        int flag = get<2>(it->second);
        if (flag != FAILED) {
            string target_node_ip = get_ip_by_id(target_node_id);
            this->socket_util->send_message(msg, target_node_ip, msg_type);
        }
    }
}

void Node::leader_election() {
    /*
    Goal: select the node with smaller hash_id as the new master
    1. Create vector with ip_addr of all nodes with lower hash_id
    2. If the vector is empty, send "NEW_LEADER" to all nodes
    3. If the vector is not empty, send "ELECTION" to all nodes with lower id
    */
    
    // int counter = 0;

    string msg = "[MASTER ELECTION] Starting master election with hash_id = " + to_string(this->hash_id);

    vector<string> pending_elections;
    vector<string> target_nodes;
    map<int, string>::iterator it;
    for(it = this->hash_ring.begin(); it != this->hash_ring.end(); it++) {
        int node_hash = it->first;
        string node_id = it->second;
        string node_ip_addr = get_ip_by_id(node_id);
        if (node_ip_addr == " ") {
            continue;
        }

        
        if (node_hash < this->hash_id) {
            // counter += 1;
            pending_elections.push_back(node_ip_addr);
            // string hash_str = to_string(this->hash_id);
           
        } else {
            if (node_hash != this->hash_id) {
                target_nodes.push_back(node_ip_addr);
            }
        }
    }
    if (pending_elections.size() == 0) {
        string cur_node_ip = get_ip_by_id(this->id);
        broadcast(this->id, NEW_MASTER);
        // reelection_status = MASTER_SELECTED;
        // this->is_master = 1;
        // master_ip = get_ip_by_id(this->id);
        this->new_master_setup(this->id);
        string msg = "[NEW MASTER] Set node with hash_id = " + to_string(this->hash_id);
        this->node_logger->log(msg);
        cout << msg << endl;
        // TODO: clean up files and setup master file table etc...

    } else{
        cout <<"[ELECTION INFO] My hash_id: " + this->hash_id << endl;
        for (int i = 0; i < pending_elections.size(); i++) {
             this->socket_util->send_message(this->id, pending_elections[i], ELECTION);
             cout << "[ELECTION SENT] ELECTION Message sent to node ip : " + pending_elections[i] << endl;
        }
        this->reelection_status = WAITING_ELECTION_RES;
        sleep(ELECTION_TIMEOUT);
        if (reelection_status != WAITING_NEW_MASTER) {
            for (int i = 0; i < target_nodes.size(); i++) {
                this->socket_util->send_message(this->id, target_nodes[i], NEW_MASTER);
            }
            reelection_status = MASTER_SELECTED;
            this->is_master = 1;
            master_ip = get_ip_by_id(this->id);
            // TODO: clean up files and setup master file table etc...
            string msg = "[NEW MASTER] Set node with hash_id = " + to_string(this->hash_id);
            this->node_logger->log(msg);
            cout << msg << endl;
        }
        // TODO: Check pending_election vector
        /*
            1. If the size of the vector doesn't change
                -- Nodes with lower hash_id aren't active
                -- Send NEW_MASTER message to all other nodes
            2. If the size does change
                -- Received ELECTION_RES message from one or more nodes with lower hash_id
                -- Wait for NEW_MASTER message
                -- Start new ELECTION if no answer within a time out
        */
    }
}



void Node::put_file(string local_name, string sdfs_name) {
    // Send sdfs_name to the master to check for update or add new file
    // Master reply with file_object info and replicas to send the file
    int send_ret = this->socket_util->send_message(sdfs_name, master_ip, PUT_REQ);
}