#include "../header_files/Node.h"

string get_ip() {
    struct ifaddrs *ifap, *ifa;
    struct sockaddr_in *sa;
    char* addr;
    int success_flag = 0;

    getifaddrs(&ifap);
    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && (ifa->ifa_addr->sa_family == AF_INET)) {
            if (strcmp(ifa->ifa_name, INTERFACE) == 0) {
                success_flag = 1;
                sa = (struct sockaddr_in*)ifa->ifa_addr;
                addr = inet_ntoa(sa->sin_addr);
                break;
            }
        }
    }
    if (success_flag == 0) {
        cout << "[ERROR] Failed to get IP address"<<endl;
        exit(1);
    }
    string result(addr);
    return result;
}

string create_id(){
    string ip_addr = get_ip();
    time_t current_time;
    stringstream ss;
    ss << current_time;
    string join_time = ss.str();
    string result = ip_addr + "::" + PORT + "::" + join_time;
    return result;
}

int get_hash_by_id(string node_id) {
    // time_t current_time;
    // stringstream ss;
    // ss << current_time;
    // string hash_param = ss.str();

    vector<string> parse_node_id = splitString(node_id, "::");
    if (parse_node_id.size() != 3) {
        cout << "[ERROR] Invalid node_id. Unable to get hash_id" << endl;
        exit(1);
    }
    string hash_param = parse_node_id[2]; // Use join time of the node as hash parameter

    hash<string> str_hash;
    int hash_ret = str_hash(hash_param);
    if (hash_ret < 0) {
        hash_ret = - hash_ret;
    }
    int node_hash = hash_ret % HASH_MOD;

    return node_hash;
}

string get_ip_by_id(string node_id) {
    //ip::port::join_time
    vector<string> parse_id = splitString(node_id, "::");
    if (parse_id.size() != 3) {
        cout <<"[ERROR] failed to get ip due to invalid node id format: "<< node_id<<endl;
        return " ";
    }
    return parse_id[0];
}

vector<string> splitString(string s, string delim) {
    // cout <<"[DEBUG SPLIT_STRING] s: " << s << " delim: "<< delim <<endl;
    vector<string> result;
    int delim_size = delim.length();
    int prev = 0;
    size_t pos;
    string sub = s;
    string buf;
    while (pos != string::npos) {
        pos = sub.find(delim);
        if (pos == string::npos) {
            // cout << "[DEBUG SPLIT_STRING] prev: " << prev << endl;
            // cout << "[DEBUG SPLIT_STRING] sub: " << sub << endl;
            result.push_back(sub);
            break;
        }
        buf = sub.substr(prev, (int)pos - prev);
        prev = (int)pos + delim_size;
        result.push_back(buf);
        sub = sub.substr(prev);
        prev = 0;
        // cout << "[DEBUG SPLIT_STRING] pos: " << pos <<endl;
    }
    // cout << "[DEBUG SPLIT_STRING] result size: "<< result.size()<<endl;
    
    return result;
}