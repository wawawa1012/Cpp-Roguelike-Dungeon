#include <iostream>
#include <cmath>
#include <windows.h>
#include <algorithm>
#include <conio.h> // 用于 _getch() 捕获键盘
#include "DungeonGenerator.h"

using namespace std;

void setColor(int colorCode) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, colorCode);
}
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
DungeonGenerator::DungeonGenerator(Player& player,int floor) : Hero(player),depth(floor),rng(rd()) {
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

// 判断两点之间是否畅通（仅支持直线）
bool isLineOfSightClear(const vector<vector<char>>& map, int x1, int y1, int x2, int y2) {
    if (x1 != x2 && y1 != y2) return false; // 不在同一直线，无法射击

    if (x1 == x2) { // 垂直方向
        int startY = min(y1, y2) + 1;
        int endY = max(y1, y2);
        for (int y = startY; y < endY; ++y) {
            if (map[y][x1] == '#') return false; // 被墙挡住了
        }
    } else { // 水平方向
        int startX = min(x1, x2) + 1;
        int endX = max(x1, x2);
        for (int x = startX; x < endX; ++x) {
            if (map[y1][x] == '#') return false;
        }
    }
    return true;
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
        else if (room.type == RoomType::BOSS)
        {
            map[centerY][centerX] = 'B';
            Enemy newEnemy={centerX,centerY,30+(depth-1)*15,4+(depth-1)*2,'B',50};
            enemies.push_back(newEnemy);
            map[newEnemy.y][newEnemy.x] = newEnemy.symbol;
        }
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
                uniform_int_distribution<int> typeDist(1, 100);
                char symbol = (typeDist(rng) <= 30) ? 'A' : 'E';
                // 弓箭手血量脆一点，攻击力稍微高一点
                int hp = (symbol == 'A') ? 5 + (depth - 1) * 3 : 10 + (depth - 1) * 5;
                int atk = (symbol == 'A') ? 3 + (depth - 1) * 2 : 2 + (depth - 1) * 1;
                Enemy newEnemy = {enemyX(rng), enemyY(rng), hp, atk, symbol, 5 + (depth - 1) * 2};
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
                // 1. 渲染主角 (最优先)
                if (x == Hero.x && y == Hero.y) {
                    setColor(10); // 10 是高亮绿色
                    cout << "@ ";
                    setColor(7);  // 7 是默认白色，【极其关键】每次用完必须洗掉刷子！
                }
                else {
                    // 2. 渲染黑夜
                    if (fovMap[y][x] == 0) {
                        cout << "  ";
                    }
                    // 3. 渲染迷雾记忆 (不在视野内)
                    else if (fovMap[y][x] == 1) {
                        setColor(8); // 8 是暗灰色，营造一种“过去”的氛围感
                        if (map[y][x] == 'E' || map[y][x] == 'B' || map[y][x] == 'A') { // 连同弓箭手一起遮蔽
                            cout << ". "; // 掩耳盗铃，隐藏怪物
                        } else {
                            cout << map[y][x] << " ";
                        }
                        setColor(7); // 恢复白色
                    }
                    // 4. 渲染真实视野 (万物显形！)
                    else {
                        char tile = map[y][x]; // 先把这个格子的东西拿在手里

                        // 核心：根据不同物体换刷子
                        if (tile == 'E') setColor(12);      // 12 是高亮红色 (危险小怪)
                        else if (tile == 'A') setColor(12); // 射手也是红色
                        else if (tile == 'B') setColor(13); // 13 是紫色 (深渊Boss)
                        else if (tile == 'T') setColor(14); // 14 是金黄色 (宝藏盲盒)
                        else if (tile == '>') setColor(11); // 11 是青色 (传送门/下楼)
                        else setColor(7);                   // 墙壁和地板保持白色

                        cout << tile << " "; // 上好色后，狠狠地印上去
                        setColor(7);         // 再次洗掉刷子
                    }
                }
            }
            cout << endl;
        }
        cout << "HP: " << Hero.hp << "/" << Hero.maxHp
        <<" | MP: "<<Hero.mp<<"/"<<Hero.maxMp
     << " | ATK: " << Hero.atk
     << " | 当前宝藏数: " << Hero.score<<" | 当前等级: "<<Hero.level
     <<" | 当前经验值: "<<Hero.exp<< "/" << Hero.maxExp
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

        switch (input)
        {
        case 'w':
        case 'W': targetY -= 1;
            break;
        case 's':
        case 'S': targetY += 1;
            break;
        case 'a':
        case 'A': targetX -= 1;
            break;
        case 'd':
        case 'D': targetX += 1;
            break;
        case 'j':
        case 'J': {
                if (Hero.mp >= 10) {
                    Hero.mp -= 10;
                    addLogMessage("你释放了剑刃风暴！席卷了周围的黑暗！");

                    // 核心重构：倒序遍历敌人数组！这是 C++ 中安全使用 erase() 删除多个元素的高级技巧
                    for (int i = enemies.size() - 1; i >= 0; i--) {
                        // 判断怪物是否在主角的九宫格内 (X和Y的曼哈顿距离拆分：也就是绝对差值都 <= 1)
                        if (abs(enemies[i].x - Hero.x) <= 1 && abs(enemies[i].y - Hero.y) <= 1) {

                            enemies[i].hp -= (Hero.atk * 2); // 造成双倍真实伤害

                            if (enemies[i].hp <= 0) { // 怪物死亡结算
                                int gainedExp = enemies[i].expReward;

                                // 精准擦除死亡怪物原本的坐标！
                                if (enemies[i].symbol == 'E' || enemies[i].symbol == 'A') { // 修复：射手死亡正确清空残影
                                    addLogMessage("黑暗子民在剑刃风暴中化作了尘埃...");
                                    map[enemies[i].y][enemies[i].x] = '.';
                                } else if (enemies[i].symbol == 'B') {
                                    addLogMessage("Boss 被剑刃风暴绞杀，通往深渊的阶梯出现了！");
                                    map[enemies[i].y][enemies[i].x] = '>';
                                }

                                enemies.erase(enemies.begin() + i); // 倒序删除，绝对安全
                                Hero.exp += gainedExp;
                            }
                        }
                    }
                    // 扫尾工作：经验统一加完后，统一结算升级（防止一次杀多只怪刷屏日志）
                    Hero.checkLevelUp(messageLog);
                }
                else {
                    addLogMessage("法力值不足！");
                }
                break;
        }
        case 'r':
        case 'R':
            {
                Hero.mp=min(Hero.maxMp,Hero.mp+5);
                addLogMessage("你深吸一口气，回复了 5 点法力");
            }
            break;
        }

        // 如果玩家试图移动了
        if (targetX != Hero.x || targetY != Hero.y) {
            // 修复：修复了多余的 ) 并且加入了 'A' 的碰撞判定
            if (map[targetY][targetX] == 'E' || map[targetY][targetX] == 'B' || map[targetY][targetX] == 'A')
            {
                for (int i = 0; i < enemies.size(); i++)
                {
                    if (enemies[i].x == targetX && enemies[i].y == targetY)
                    {
                        enemies[i].hp -= Hero.atk;
                        addLogMessage("英雄挥剑，对黑暗子民造成了 " + std::to_string(Hero.atk) + " 点伤害！");
                        if (enemies[i].hp <= 0) //怪物死亡逻辑
                        {
                            int gainedExp = enemies[i].expReward;
                            if (enemies[i].symbol == 'E' || enemies[i].symbol == 'A'){ // 修复：射手死亡正确清空残影
                                addLogMessage("黑暗子民化作了尘埃...");
                                map[targetY][targetX] = '.';
                            }
                            else if (enemies[i].symbol == 'B')
                            {
                                addLogMessage("Boss 轰然倒塌，通往深渊的阶梯出现了！");
                                map[targetY][targetX] = '>';
                            }
                            enemies.erase(enemies.begin() + i);
                            Hero.exp += gainedExp;
                            Hero.checkLevelUp(messageLog);
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
            uniform_int_distribution<int> lootDist(1, 100);
            int roll = lootDist(rng);
            if (roll>0&&roll<=40)
            {
                Hero.hp=min(Hero.maxHp,Hero.hp+50);
                addLogMessage("你饮下生命药水，恢复了 50 点生命！");
            }
            else if (roll>40&&roll<=70)
            {
                Hero.atk+=3;
                addLogMessage("你捡起战神磨刀石，武器变得更加锋利！ATK +3");
            }
            else
            {
                Hero.maxHp+=20;
                Hero.hp+=20;
                addLogMessage("你融合了巨人之心！最大生命值 +20！");
            }
            map[Hero.y][Hero.x] = '.';
            Hero.score+=1;
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
            if (dist > 1 && dist < 8)
            {
                // ========== 弓箭手 AI 分支 ==========
                if (enemies[i].symbol == 'A' && isLineOfSightClear(map, enemies[i].x, enemies[i].y, Hero.x, Hero.y)) {
                    // 1. 扣血结算
                    Hero.hp -= enemies[i].atk;
                    addLogMessage("弓箭手在暗处射出了一箭！失去 " + std::to_string(enemies[i].atk) + " HP！");

                    // 2. 渲染瞬间的激光弹道 (肉鸽爽感来源)
                    setColor(14); // 黄色激光
                    if (enemies[i].x == Hero.x) { // 垂直射击
                        for (int y = min(enemies[i].y, Hero.y) + 1; y < max(enemies[i].y, Hero.y); ++y) {
                            setCursorPosition(enemies[i].x * 2, y); // 注意控制台x坐标通常需要 *2 适应字符宽度，具体看你的排版
                            cout << "|";
                        }
                    } else { // 水平射击
                        for (int x = min(enemies[i].x, Hero.x) + 1; x < max(enemies[i].x, Hero.x); ++x) {
                            setCursorPosition(x * 2, enemies[i].y);
                            cout << "-";
                        }
                    }
                    setColor(7);
                    Sleep(50); // 极短的停顿，让玩家肉眼能捕捉到这道激光残影！

                    continue; // 射击完毕，本回合不再移动
                }

                // ========== 经典的贪心寻路 AI (所有怪共用) ==========
                int nextX = enemies[i].x, nextY = enemies[i].y;
                int distX = Hero.x - enemies[i].x;
                int distY = Hero.y - enemies[i].y;
                // 优先走距离差距更大的轴
                if (abs(distX) > abs(distY)) {
                    if (distX > 0) nextX += 1; else if (distX < 0) nextX -= 1;
                } else {
                    if (distY > 0) nextY += 1; else if (distY < 0) nextY -= 1;
                }

                if (map[nextY][nextX] == '.') {
                    map[enemies[i].y][enemies[i].x] = '.';
                    enemies[i].x = nextX; enemies[i].y = nextY;
                    map[nextY][nextX] = enemies[i].symbol;
                }
            }
        } // 修复：这里补全了之前丢失的用来结束敌人 for 循环的大括号！

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

    return false; // 安全垫：防止编译器报 "control reaches end of non-void function"
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
        DungeonGenerator dungeon(mainHero,currentFloor);
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