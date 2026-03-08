#include <iostream>
#include <cmath>
#include <windows.h>
#include <algorithm>
#include <conio.h> // 用于 _getch() 捕获键盘
#include "DungeonGenerator.h"

using namespace std;

// Windows API：将控制台光标瞬间移动到指定坐标 (x, y)，解决闪屏Bug
void setCursorPosition(int x, int y) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD pos;
    pos.X = x;
    pos.Y = y;
    SetConsoleCursorPosition(hConsole, pos);
}

// 隐藏控制台原本那个一直闪烁的小下划线光标（让画面更干净）
void hideCursor() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = false; // 设为不可见
    SetConsoleCursorInfo(hConsole, &cursorInfo);
}

// 反向思维的 AABB 碰撞检测：判断两个房间是否重叠（包含1格缓冲带）
bool isOverlap(const Room& room1, const Room& room2) {
    bool isLeftOf = (room1.x + room1.width + 1) < room2.x;
    bool isRightOf = room1.x > (room2.x + room2.width + 1);
    bool isAbove = (room1.y + room1.height + 1) < room2.y;
    bool isBelow = room1.y > (room2.y + room2.height + 1);
    return !(isLeftOf || isRightOf || isAbove || isBelow);
}

// 按边权比较（升序）
bool compareByWeight(const Edge& e1, const Edge& e2) {
    return e1.weight < e2.weight;
}

// ==========================================
// 并查集 (UnionFind) 成员函数实现
// ==========================================
UnionFind::UnionFind(int n) {
    parent.resize(n);
    for (int i = 0; i < n; ++i) parent[i] = i;
}

int UnionFind::find(int i) {
    if (parent[i] == i) return i;
    else return parent[i] = find(parent[i]);
}

bool UnionFind::unite(int i, int j) {
    int rootI = find(i);
    int rootJ = find(j);
    if (rootI == rootJ) return false;
    else parent[rootI] = rootJ;
    return true;
}

// ==========================================
// 类的成员函数实现
// ==========================================
DungeonGenerator::DungeonGenerator(Player& player) : Hero(player),rng(rd()) {
    // 初始化地图，全部填满墙壁 '#'
    map.resize(MAP_HEIGHT, vector<char>(MAP_WIDTH, '#'));
    fovMap.resize(MAP_HEIGHT, vector<int>(MAP_WIDTH, 0));
}

// 计算房间 A 和房间 B 中心点之间的曼哈顿距离
int DungeonGenerator::calculateManhattanDistance(const Room& a, const Room& b) const {
    // 修复后的中心点数学逻辑：起始点 + 长度的一半
    int aCenterX = a.x + a.width / 2;
    int aCenterY = a.y + a.height / 2;
    int bCenterX = b.x + b.width / 2;
    int bCenterY = b.y + b.height / 2;
    return abs(aCenterX - bCenterX) + abs(aCenterY - bCenterY);
}

// 挖掘 L 型走廊连接两个房间
void DungeonGenerator::digCorridor(const Room& roomA, const Room& roomB) {
    int startX = roomA.x + roomA.width / 2;
    int startY = roomA.y + roomA.height / 2;
    int endX = roomB.x + roomB.width / 2;
    int endY = roomB.y + roomB.height / 2;

    uniform_int_distribution<int> coinToss(0, 1);
    bool horizontalFirst = (coinToss(rng) == 0); // 必须在类里面，因为用到了类的 rng

    if (horizontalFirst) {
        for (int x = std::min(startX, endX); x <= std::max(startX, endX); ++x) map[startY][x] = '.';
        for (int y = std::min(startY, endY); y <= std::max(startY, endY); ++y) map[y][endX] = '.';
    } else {
        for (int y = std::min(startY, endY); y <= std::max(startY, endY); ++y) map[y][startX] = '.';
        for (int x = std::min(startX, endX); x <= std::max(startX, endX); ++x) map[endY][x] = '.';
    }
}

void DungeonGenerator::updateFOV()
{
    for (int y=0;y<MAP_HEIGHT;y++)
    {
        for (int x=0;x<MAP_WIDTH;x++)
        {
            if (fovMap[y][x]==2)
            {
                fovMap[y][x]=1;//降级为visited
            }
        }
    }
    int radius = 5; //视野半径
    int leftMost=max(Hero.x-radius,0);
    int rightMost=min(Hero.x+radius,MAP_WIDTH);
    int highest=max(Hero.y-radius,0);
    int lowest=min(Hero.y+radius,MAP_HEIGHT);
    for (int y=highest;y<lowest;y++)
    {
        for (int x=leftMost;x<rightMost;x++)
        {
            fovMap[y][x]=2;
        }
    }
}

void DungeonGenerator::addLogMessage(const std::string& msg)
{
    messageLog.push_back(msg);
    if (messageLog.size()>3)
    {
        messageLog.erase(messageLog.begin()); //fifo队列
    }
}

void DungeonGenerator::generateRooms() {
    uniform_int_distribution<int> sizeDist(4, 8);

    for (int i = 0; i < MAX_ROOMS; ++i) {
        Room newRoom;
        bool roomValid = false;
        int attempts = 0;

        // 核心安全锁：拒绝采样 + 熔断机制
        while (!roomValid && attempts < MAX_ATTEMPTS) {
            attempts++;
            newRoom.width = sizeDist(rng);
            newRoom.height = sizeDist(rng);

            // 确保房间不会生成在地图外面
            uniform_int_distribution<int> xDist(1, MAP_WIDTH - newRoom.width - 1);
            uniform_int_distribution<int> yDist(1, MAP_HEIGHT - newRoom.height - 1);
            newRoom.x = xDist(rng);
            newRoom.y = yDist(rng);

            roomValid = true;
            for (const auto& existingRoom : rooms) {
                if (isOverlap(newRoom, existingRoom)) {
                    roomValid = false;
                    break;
                }
            }
        }

        if (roomValid) {
            // 挖掘房间地板
            for (int y = newRoom.y; y < newRoom.y + newRoom.height; ++y) {
                for (int x = newRoom.x; x < newRoom.x + newRoom.width; ++x) {
                    map[y][x] = '.';
                }
            }
            rooms.push_back(newRoom);
        } else {
            cout << "警告：地图空间极度拥挤！在生成第 " << i + 1 << " 个房间时触发熔断，提前结束生成。" << endl;
            break;
        }
    }

    // ==========================================
    // Day 2 核心：构建图论模型（计算所有可能连线的边权）
    // ==========================================
    int numRooms = rooms.size();
    for (int i = 0; i < numRooms; ++i) {
        for (int j = i + 1; j < numRooms; ++j) {
            int dist = calculateManhattanDistance(rooms[i], rooms[j]);
            edges.push_back({i, j, dist});
        }
    }

    // 核心排序：找到最短的潜在走廊
    sort(edges.begin(), edges.end(), compareByWeight);

    // 并查集登场：拒绝死循环环路！
    UnionFind uf(rooms.size());
    for (Edge e : edges) {
        if (uf.unite(e.u, e.v)) {
            digCorridor(rooms[e.u], rooms[e.v]);
        }
    }

    // ==========================================
    // Day 4: 赋予房间语义与黑暗子民降临
    // ==========================================
    rooms[0].type = RoomType::START;

    int maxDist = -1;
    int bossRoomIndex = 0;

    // 1. 找最远房间确定BOSS房
    for (int i = 1; i < rooms.size(); i++) {
        int dist = calculateManhattanDistance(rooms[0], rooms[i]);
        if (dist > maxDist) {
            maxDist = dist;
            bossRoomIndex = i;
        }
    }
    rooms[bossRoomIndex].type = RoomType::BOSS;

    // 2. 剩余的房间抽卡决定是否为宝箱房 (20%概率)
    uniform_int_distribution<int> treasureToss(1, 100);
    for (int i = 1; i < rooms.size(); i++) {
        if (rooms[i].type == RoomType::BOSS) continue;
        if (treasureToss(rng) <= 20) {
            rooms[i].type = RoomType::TREASURE;
        }
    }

    // 3. 将房间的身份“刻”在地图数组的中心位置
    for (const auto& room : rooms) {
        int centerX = room.x + room.width / 2;
        int centerY = room.y + room.height / 2;

        if (room.type == RoomType::START) map[centerY][centerX] = 'S';
        else if (room.type == RoomType::BOSS) map[centerY][centerX] = 'B';
        else if (room.type == RoomType::TREASURE) map[centerY][centerX] = 'T';
    }

    // 4. 把英雄放在0号房间中心
    Hero.x = rooms[0].x + rooms[0].width / 2;
    Hero.y = rooms[0].y + rooms[0].height / 2;

    // 5. 怪物部署逻辑：利用 for 循环避免代码冗余 (DRY原则)
    uniform_int_distribution<int> countDist(1, 3); // 每个房间刷 1-3 只怪

    for (const auto& room : rooms) {
        // 只在普通房刷普通怪
        if (room.type == RoomType::NORMAL) {
            uniform_int_distribution<int> enemyX(room.x, room.x + room.width - 1);
            uniform_int_distribution<int> enemyY(room.y, room.y + room.height - 1);

            int monsterCount = countDist(rng);

            for (int i = 0; i < monsterCount; ++i) {
                Enemy newEnemy = {enemyX(rng), enemyY(rng), 10, 2, 'E'};
                enemies.push_back(newEnemy);
                map[newEnemy.y][newEnemy.x] = newEnemy.symbol;
            }
        }
    }
}

void DungeonGenerator::printMap() const {
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < MAP_WIDTH; ++x) {
            cout << map[y][x] << " ";
        }
        cout << endl;
    }
}

// 供调试使用的辅助函数：打印生成的边数
void DungeonGenerator::printEdgesCount() const {
    cout << "构建图论模型完毕！共生成有效房间: " << rooms.size()
        << " 个，生成潜在走廊连线: " << edges.size() << " 条。" << endl;
}

// ==========================================
// 引擎心脏：主游戏循环 (Game Loop)
// ==========================================
bool DungeonGenerator::play()
{
    hideCursor();

    while (true) {
        updateFOV();
        // 1. Render (渲染阶段)
        setCursorPosition(0,0);

        // 动态覆盖打印：如果是英雄的坐标，就打印 '@'，否则打印地图原本的字符
        for (int y = 0; y < MAP_HEIGHT; ++y) {
            for (int x = 0; x < MAP_WIDTH; ++x) {
                if (x == Hero.x && y == Hero.y) {
                    cout << "@ "; // 英雄的化身
                } else {
                    if (fovMap[y][x]==0)  //未知
                    {
                        cout<<"  ";
                    }
                    else if (fovMap[y][x]==1) //已访问但不在视野内
                    {
                        if (map[y][x]=='E'||map[y][x]=='B')
                        {
                            cout<<". ";
                        }
                        else cout<<map[y][x]<<" ";
                    }
                    else cout<<map[y][x]<<" ";
                }
            }
            cout << endl;
        }
        cout << "HP: " << Hero.hp << "/" << Hero.maxHp
     << " | ATK: " << Hero.atk
     << " | 当前宝藏数: " << Hero.score
     << "                                      " << endl;
        // 打印固定的 3 行日志，不足 3 行用空行补齐，每行末尾加大量空格清除残影
        for (int i = 0; i < 3; ++i) {
            if (i < messageLog.size()) {
                // 打印日志，并在末尾追加 40 个空格来洗掉残影
                cout << messageLog[i] << "                                        " << endl;
            } else {
                // 如果当前没有那么多日志，也必须打印纯空格的空行来覆盖旧画面
                cout << "                                                                " << endl;
            }
        }

        // 2. Input & Update (输入与更新阶段)
        char input = _getch(); // 瞬间捕获键盘按键，不需要回车

        // 按下 Esc 键 (ASCII码 27) 退出游戏
        if (input == 27) return false;

        // 3. 处理移动与碰撞检测
        int targetX = Hero.x;
        int targetY = Hero.y;

        switch (input) {
        case 'w': case 'W': targetY -= 1; break;
        case 's': case 'S': targetY += 1; break;
        case 'a': case 'A': targetX -= 1; break;
        case 'd': case 'D': targetX += 1; break;
        }
        // 如果玩家试图移动了
        if (targetX != Hero.x || targetY != Hero.y) {
            if (map[targetY][targetX] == 'E')
            {
                for (int i = 0; i < enemies.size(); i++)
                {
                    if (enemies[i].x == targetX && enemies[i].y == targetY)
                    {
                        enemies[i].hp -= Hero.atk;
                        addLogMessage("英雄挥剑，对黑暗子民造成了 " + std::to_string(Hero.atk) + " 点伤害！");
                        if (enemies[i].hp <= 0)
                        {
                            enemies.erase(enemies.begin() + i);
                            addLogMessage("黑暗子民化作了尘埃...");
                            map[targetY][targetX] = '.';
                        }
                        break;
                    }
                }
            }
            // 向目标方向走
            else if (map[targetY][targetX] != '#')
            {
                Hero.y=targetY;Hero.x=targetX;
                if (map[Hero.y][Hero.x]=='>') return true;  //进入下一层
            }
        }
        // 拾取宝藏逻辑
        if (map[Hero.y][Hero.x]=='T') {
            Hero.score+=1;
            addLogMessage("你发现了一个闪闪发光的宝箱！");
            map[Hero.y][Hero.x]='.';
        }

        // ==========================================================
        // 【敌人回合开始】
        // ==========================================================
        for (int i=0;i<enemies.size();i++)
        {
            int dist=abs(Hero.x-enemies[i].x)+abs(Hero.y-enemies[i].y);
            if (dist>=8) continue;  //超出视野范围
            if (dist==1)        //距离为一开始对战
            {
                Hero.hp-=enemies[i].atk;
                addLogMessage("黑暗子民在黑暗中袭击了你！失去 " + std::to_string(enemies[i].atk) + " HP！");
                continue;
            }
            if (dist>1&&dist<8)
            {
                int nextX = enemies[i].x, nextY = enemies[i].y;
                int distX=Hero.x-enemies[i].x;
                int distY=Hero.y-enemies[i].y;
                // 决定先走 X 轴还是先走 Y 轴 (优先缩短距离差距更大的那条轴)
                if (abs(distX) > abs(distY))
                {
                    // 走 X 轴
                    if (distX > 0) nextX += 1;       // Hero 在右，往右走
                    else if (distX < 0) nextX -= 1;  // Hero 在左，往左走
                }
                else
                {
                    // 走 Y 轴
                    if (distY > 0) nextY += 1;       // Hero 在下，往下走
                    else if (distY < 0) nextY -= 1;  // Hero 在上，往上走
                }
                if (map[nextY][nextX]=='.')  //限制活动范围
                {
                    map[enemies[i].y][enemies[i].x]='.';
                    enemies[i].x=nextX;enemies[i].y=nextY;
                    map[nextY][nextX]=enemies[i].symbol;
                }
            }
        }
        // =====================================
        // 在怪物回合结束之后，添加死亡结算逻辑
        // =====================================
        if (Hero.hp <= 0) {
            system("cls"); // 瞬间清空画面
            cout << "\n\n\n";
            cout << "      ======================================" << endl;
            cout << "      =                                    =" << endl;
            cout << "      =             YOU DIED               =" << endl;
            cout << "      =                                    =" << endl;
            cout << "      ======================================" << endl;
            cout << "\n      被黑暗子民永远地留在了地牢深处..." << endl;
            cout << "\n      最终带着宝藏数: " << Hero.score << " 饮恨西北。" << endl;
            cout << "\n      按任意键接受命运..." << endl;

            _getch(); // 挂起，让玩家绝望地看着死亡画面
            return false;   // 【极其关键】用 return 直接彻底终结整个 play() 函数！跳回主函数！
        }
    }
}
int main() {
    SetConsoleOutputCP(CP_UTF8);

    // 1. 英雄降临！玩家的生命周期现在和整个游戏一样长了
    Player mainHero;
    int currentFloor = 1; // 记录当前层数

    while (true) {
        system("cls");
        cout << "--- Hades2：地牢生成器初始化 --- 深入层数: " << currentFloor << endl;

        // 2. 依赖注入：把我们的 mainHero 塞进新生成的平行宇宙里
        DungeonGenerator dungeon(mainHero);
        dungeon.generateRooms();

        cout << "\n按任意键踏入地牢..." << endl;
        _getch();

        // 3. 状态机：接收 play() 的审判结果
        bool survived = dungeon.play();

        if (!survived) {
            break; // 死了或者主动退出，直接打破轮回
        }

        // 能活着走到这一行，说明下楼了。层数+1，自然进入下一次循环
        currentFloor++;
    }

    return 0;
}