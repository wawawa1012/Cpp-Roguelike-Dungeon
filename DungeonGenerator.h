#pragma once

#include <vector>
#include <random>        // 类成员需要确切的大小
#include <string>
#include "CoreStructs.h" // 依赖底层基石

struct UnionFind {
    std::vector<int> parent;
    UnionFind(int n);
    int find(int i);
    bool unite(int i, int j);
};

bool isOverlap(const Room& room1, const Room& room2);
bool compareByWeight(const Edge& e1, const Edge& e2);

class DungeonGenerator {
private:
    std::vector<std::vector<char>> map;
    std::vector<Room> rooms;
    std::vector<Edge> edges;
    Player& Hero;
    std::vector<Enemy> enemies; //用来存储全图所有怪物的数组
    std::vector<std::string> messageLog;
    int depth; //记录层数
    
    // 随机数引擎必须在类声明中占位，保证内存布局完整
    std::random_device rd;
    std::mt19937 rng;

    int calculateManhattanDistance(const Room& a, const Room& b) const;
    void digCorridor(const Room& roomA, const Room& roomB);
    std::vector<std::vector<int>> fovMap; // Field of View Map (视野地图)
    void updateFOV();
    void addLogMessage(const std::string& msg);

public:
    DungeonGenerator(Player& player,int floor);
    void generateRooms();
    void printMap() const;
    void printEdgesCount() const;
    bool play(); //day4新增，主游戏循环
};