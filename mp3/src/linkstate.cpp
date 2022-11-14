#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <set>

#define MAXSIZE 100000

using namespace std;

typedef struct Message
{
    int source;
    int dest;
    string content;
    Message(int source, int dest, string message)
    {
        source = source;
        dest = dest;
        content = message;
    }
} Message;
vector<Message> message_list;
map<int, map<int, pair<int, int>>> forward_table; // source, destionation, <next, cost>

// set<int> nodes;
vector<vector<int>> graph;
int nodes_cnt; // number of nodes

void init_graph(ifstream &topo_file)
{
    int source, dest, cost;
    while (topo_file >> source >> dest >> cost)
    {
        // nodes.insert(source);
        // nodes.insert(dest);
        graph[source][dest] = cost;
        graph[dest][source] = cost;
    }
    nodes_cnt = graph.size();

    // construct the graph
    for (int row = 0; row < nodes_cnt + 1; row++)
    {
        vector<int> v1;
        for (int col = 0; col < nodes_cnt + 1; col++)
        {
            if (row == col)
                v1.push_back(0);
            else
                v1.push_back(-999);
        }
        graph.push_back(v1);
        v1.clear();
    }
}

void read_message(ifstream &msg_file)
{
    int source, dest;
    string line, message_str;
    while (msg_file)
    {
        getline(msg_file, line);
        sscanf(line.c_str(), "%d %d %*s", &source, &dest);
        message_str = line.substr(line.find(" "));
        message_str = message_str.substr(line.find(" ") + 1);
        Message message(source, dest, message_str);

        message_list.push_back(message);
    }
}

void send_message(fstream &fpOut)
{
    // send message
    for (int i = 0; i < message_list.size(); i++)
    {
        int source = message_list[i].source;
        int dest = message_list[i].dest;
        string content = message_list[i].content;

        int nextHop = source;
        if (forward_table.find(source) == forward_table.end() ||
            forward_table[source].find(dest) == forward_table[source].end())
        { // The destination is not reachable.
            fpOut << "from " << source << " to " << dest << " cost infinite hops unreachable message"
                  << content << endl;
        }
        else
        {
            fpOut << "from " << source << " to " << dest << " cost " << forward_table[source][dest].second;
            fpOut << " hops ";
            while (nextHop != dest)
            {
                fpOut << nextHop << " ";
                nextHop = forward_table[nextHop][dest].first;
            }
            fpOut << "message" << content << endl;
        }
    }
}

void dijkstra(int node)
{

}

void get_shortest_path(fstream &fpOut){
    for (int node = 1; node < nodes_cnt + 1; node++)
    {
        dijkstra(node);
        fpOut << endl;
    }
}

void process_change(ifstream &change_file, fstream &fpOut)
{
    int source, dest, cost;
    while (change_file >> source >> dest >> cost)
    {
        graph[source][dest] = cost;
        graph[dest][source] = cost;
        forward_table.clear();
        get_shortest_path(fpOut);
        send_message(fpOut);
    }
}

int main(int argc, char **argv)
{
    // printf("Number of arguments: %d", argc);
    if (argc != 4)
    {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }
    string topo_file_name = argv[1];
    string msg_file_name = argv[2];
    string change_file_name = argv[3];
    
    fstream fpOut;
    fpOut.open("output.txt");

    // Topo file - Initialization
    ifstream topo_file(topo_file_name);
    init_graph(topo_file);
    topo_file.close();

    // Message File - Send all messages
    ifstream msg_file(msg_file_name);
    read_message(msg_file);
    send_message(fpOut);
    msg_file.close();

    // Change File
    ifstream change_file(change_file_name);
    process_change(change_file, fpOut);
    change_file.close();

    fpOut.close();
    return 0;
}
