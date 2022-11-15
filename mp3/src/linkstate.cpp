#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <map> // for implementing graph
#include <set> 
#include <queue> // for implementing queue
#include <algorithm> // for reverse

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

int nodes_cnt; // number of nodes
set<int> nodes; // nodes' num
vector<vector<int> > graph;
ofstream fpOut;

void count_nodes(ifstream &topo_file)
{
    int source, dest, cost;
    while (topo_file >> source >> dest >> cost)
    {
        nodes.insert(source);
        nodes.insert(dest);
    }
    cout << "Nodes count: " << nodes.size() << "\n";
    nodes_cnt = nodes.size();
}

void init_graph(ifstream &topo_file)
{
    int source, dest, cost;

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

    while (topo_file >> source >> dest >> cost)
    {
        graph[source][dest] = cost;
        graph[dest][source] = cost;
    }
}

void read_message(ifstream &msg_file)
{
    int source, dest;
    string line, message_str;
    while (getline(msg_file, line))
    {
        sscanf(line.c_str(), "%d %d %*s", &source, &dest);
        message_str = line.substr(line.find(" "));
        message_str = message_str.substr(line.find(" ") + 1);

        Message message;
        message.source = source;
        message.dest = dest;
        message.content = message_str;

        cout << message.source << " " << message.dest << " "<< message.content << "\n";

        message_list.push_back(message);
    }
}

void send_message()
{
    // send message
    for (int i = 0; i < message_list.size(); i++)
    {
        int source = message_list[i].source;
        int dest = message_list[i].dest;
        string content = message_list[i].content;

        int next = source;
        // The destination is not reachable.
        if (forward_table.find(source) == forward_table.end() ||
            forward_table[source].find(dest) == forward_table[source].end())
        {
            fpOut << "from " << source << " to " << dest << " cost infinite hops unreachable message" << content << endl;
        }
        else
        {
            fpOut << "from " << source << " to " << dest << " cost " << forward_table[source][dest].second;
            fpOut << " hops ";
            while (next != dest)
            {
                fpOut << next << " ";
                next = forward_table[next][dest].first;
            }
            fpOut << "message" << content << endl;
        }
    }
}

vector<int> get_neighbor(int v)
{
    vector<int> neighbors;
    for (int node = 1; node < nodes_cnt + 1; node++)
    {
        if (graph[v][node] > 0)
            neighbors.push_back(node);
    }
    return neighbors;
}

vector<int> get_path(vector<int> parent, int node, int dest)
{
    int cur = dest;
    vector<int> path;
    while (cur != node)
    {
        path.push_back(cur);
        cur = parent[cur];
    }
    path.push_back(cur);
    reverse(path.begin(), path.end());
    return path;
}

void dijkstra(int node)
{
    priority_queue<dv_pair, vector<dv_pair>, greater<dv_pair> > frontier;
    vector<int> distance(nodes_cnt + 1, INT_MAX);
    vector<int> parent(nodes_cnt + 1, node);
    vector<bool> marked(nodes_cnt + 1, false);

    distance[node] = 0;
    marked[node] = true;
    frontier.push(make_pair(0, node));
    while (!frontier.empty())
    {
        int top_node = frontier.top().second;
        frontier.pop();
        
        for (int neighbor : get_neighbor(top_node))
        {
            if (distance[neighbor] > distance[top_node] + graph[top_node][neighbor])
            {
                distance[neighbor] = distance[top_node] + graph[top_node][neighbor];
                parent[neighbor] = top_node;
                marked[neighbor] = true;

                frontier.push(make_pair(distance[neighbor], neighbor));
            }
            else if (distance[neighbor] == distance[top_node] + graph[top_node][neighbor])
            {
                marked[neighbor] = true;
                if (parent[neighbor] >= top_node)
                    parent[neighbor] = top_node;

                frontier.push(make_pair(distance[neighbor], neighbor));
            }
        }
            
    }
    

    for (int dest = 1; dest < nodes_cnt + 1; dest++)
    {
        if (marked[dest])
        { 
            // path from source to dest
            vector<int> path = get_path(parent, node, dest);
            if (node != dest)
                forward_table[node][dest] = make_pair(path[1], distance[dest]);
            else
                forward_table[node][dest] = make_pair(path[0], distance[dest]);
            fpOut << dest << " " << forward_table[node][dest].first << " " << forward_table[node][dest].second << endl;
        }
    }
}

void get_shortest_path()
{
    for (int node = 1; node < nodes_cnt + 1; node++)
    {
        dijkstra(node);
        fpOut << endl;
    }
}

void process_change(ifstream &change_file)
{
    int source, dest, cost;
    while (change_file >> source >> dest >> cost)
    {
        graph[source][dest] = cost;
        graph[dest][source] = cost;
        forward_table.clear();
        get_shortest_path();
        send_message();
    }
}

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
