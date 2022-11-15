#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <set> 
#include <queue>
#include <algorithm>

using namespace std;

typedef pair<int, int> dv_pair;
typedef struct Message
{
    int source;
    int dest;
    string content;
} Message;
vector<Message> message_list;
map<int, map<int, pair<int, int> > > forward_table; // source, destionation, <next, cost>

ofstream fpOut;



int main(int argc, char **argv)
{
    if (argc != 4)
    {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }
    string topo_file_name = argv[1];
    string msg_file_name = argv[2];
    string change_file_name = argv[3];

    fpOut.open("output.txt");

    // Topo file - Initialization
    ifstream topo_file;
    topo_file.open(topo_file_name);
    count_nodes(topo_file);
    topo_file.close();

    topo_file.open(topo_file_name);
    init_graph(topo_file);
    topo_file.close();

    print_graph();

    // Message File - Send all messages
    ifstream msg_file;
    msg_file.open(msg_file_name);
    read_message(msg_file);
    msg_file.close();

    get_shortest_path();
    send_message();

    // Change File
    ifstream change_file;
    change_file.open(change_file_name);
    process_change(change_file);
    change_file.close();

    fpOut.close();
    return 0;
}
