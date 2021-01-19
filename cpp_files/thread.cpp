#include "../header_files/Node.h"

void* server_start (void* arg) {
    Node* cur_node = (Node*)arg;
    int server_socket_create = cur_node->socket_util->create_server_socket();
    if (server_socket_create == 1) {
        cout << "[ERROR] Failed to create server" << endl;
        cur_node->node_logger->log("[ERROR] Failed to create server socket");
        exit(1);
    }
    return NULL;
}

// void* tcp_server_start(void* arg) {
//     Node* cur_node = (Node*) arg;
//     int tcp_server_socket_create = cur_node->socket_util->create_tcp_server();

// }

void* client_start(void* arg) {
    Node* cur_node = (Node*)arg;
    
    string msg = cur_node->pack_membership_list();
    msg += "$$" + to_string(cur_node->hash_id);
    if (msg.compare("ERROR") == 0) {
        cout << "[ERROR] Create membership_list" << endl;
        return NULL;
    }
    if (cur_node->membership_list.size() == 1) {
        cur_node->send_to_introducer(msg);
    }

    while (cur_node->status == ACTIVE) {
        cur_node->read_message();
        //TODO: handle switch mode case

        if (cur_node->status ==FAILED) {
            pthread_exit(NULL);
            // break;
        }
        
        pthread_mutex_lock(&cur_node->thread_lock);
        if (cur_node->dir_set == 0) {
            int create_ret = cur_node->create_sdfs_dir();
            if (create_ret != 0) {
                cout << "[ERROR] Failed to create sdfs directory" << endl;
                // exit(1); // maybe don't need to exit(1)
            } 
        }
        pthread_mutex_unlock(&cur_node->thread_lock);
        
        cur_node->failure_detection();
        cur_node->hb++;
        cur_node->local_clock++;
        cur_node->update_self_info();
        cur_node->send_hb();
        cur_node->print_members_in_system();

        // Start leader election if master is detected as failed
        if (cur_node->reelection_status == ELECTION_REQUIRED) {
            cur_node->leader_election();
        }

        sleep(T_period);

        
    /*
        1. Read message
        2. Process message
            a. Update membership list
                i. Failure detection
                ii. Remove members
            b. Switch mode
        3. Increase Hb and local_clock
        4. Send out membership list based on mode
        5. Sleep for T_period
    */
    }
    return NULL;
}