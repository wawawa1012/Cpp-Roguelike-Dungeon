#pragma once

constexpr int MAP_WIDTH = 50;
constexpr int MAP_HEIGHT = 20;
constexpr int MAX_ROOMS = 6;
constexpr int MAX_ATTEMPTS = 100;

//定义房间的身份
enum class RoomType
{
    START,   //出生点
    NORMAL, //普通房间
    TREASURE,  //宝箱房
    BOSS    //BOSS房
};
struct Room {
    int x, y;
    int width, height;
    RoomType type=RoomType::NORMAL;  //默认普通房
};

struct Edge {
    int u;      
    int v;      
    int weight; 
};

struct Player
{
    int x, y;
    int score=0;  //宝藏数
    int hp=100;
    int maxHp=100;
    int atk=5;
};
// 敌人的基础数据结构
struct Enemy {
    int x, y;
    int hp;       // 血量
    int atk;      // 攻击力
    char symbol;  // 显示符号，比如普通的 'E' (Enemy)
};