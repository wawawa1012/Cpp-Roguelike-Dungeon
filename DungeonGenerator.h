#pragma once

#include <vector>
#include <random>        // 必须包含，因为类成员需要确切的大小
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
    Player Hero;
    std::vector<Enemy> enemies; //用来存储全图所有怪物的数组
    
    // 随机数引擎必须在类声明中占位，保证内存布局完整
    std::random_device rd;
    std::mt19937 rng;

    int calculateManhattanDistance(const Room& a, const Room& b) const;
    void digCorridor(const Room& roomA, const Room& roomB);

public:
    DungeonGenerator(); 
    void generateRooms();
    void printMap() const;
    void printEdgesCount() const;
    void play(); //day4新增，主游戏循环
};