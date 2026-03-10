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
    int level = 1;
    int exp = 0;
    int maxExp = 10;
    int mp=20;
    int maxMp = 20;
    // 传入 messageLog 的引用，玩家自己往里面写升级日志！
    void checkLevelUp(std::vector<std::string>& log) {
        while (exp >= maxExp) {
            level += 1;
            exp -= maxExp;
            maxExp = static_cast<int>(1.5 * maxExp);
            atk += 2;
            maxHp += 10;
            hp = maxHp; // 升级回满血
            maxMp+=5;
            mp=maxMp;
            log.push_back("你升到了 " + std::to_string(level) + " 级！属性全面提升！");

            // 为了维持 3 行 UI 的规矩，还要在玩家这里处理一下队列长度
            if (log.size() > 3) log.erase(log.begin());
        }
    }
};
// 敌人的基础数据结构
struct Enemy {
    int x, y;
    int hp;       // 血量
    int atk;      // 攻击力
    char symbol;  // 显示符号，比如普通的 'E' (Enemy)
    int expReward=5;
};