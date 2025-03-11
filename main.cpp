#undef UNICODE

#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <String>
#include <vector>
#include <memory>
#include <algorithm>
#include <map>
#include <functional>
#include <fstream>
#include <ctype.h>
#include <filesystem>
#include <chrono>

#include "player.h"

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 4096
#define DEFAULT_PORT "4567"
#define AREAPATH "./areas/"
#define AREALISTFILE "areas.txt"
#define HELPPATH "./help/"
#define STARTING_ROOM 50
#define LEVEL_GOD 101

#define GAME_TICK_MILLISECS 1000
#define TICKS_PER_HP_GAIN 20
#define TICKS_PER_FIGHT_ROUND 2

std::string one_argument(std::string& args);

class WinSockEngine {
    
public:
    WSADATA wsaData;
    int iResult;

    WinSockEngine() {
        iResult = 0;
        // Initialize Winsock
        iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != 0) {
            printf("WSAStartup failed with error: %d\n", iResult);
        }
    }

    ~WinSockEngine() {
        WSACleanup();
    }
};

class MudSocket {
   
public:
    SOCKET sock;
    struct addrinfo* result;
    struct addrinfo hints;

    MudSocket() {
        sock = INVALID_SOCKET;
        result = NULL;
        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE;
    }

    MudSocket(MudSocket&& FromSock) : sock(FromSock.sock), result(FromSock.result), hints(FromSock.hints) {
        FromSock.sock = INVALID_SOCKET;
    }

    void operator=(MudSocket& FromSock) {
        sock = FromSock.sock;
        result = FromSock.result;
        hints = FromSock.hints;
        FromSock.sock = INVALID_SOCKET;
    }

    void operator=(MudSocket&& FromSock) {
        sock = FromSock.sock;
        result = FromSock.result;
        hints = FromSock.hints;
        FromSock.sock = INVALID_SOCKET;
    }
};

namespace MSMUD {
    std::string mudName = "MSMUD";
    std::string version = "v0.1";
}

const std::string reset = "\x1B[0m";
const std::string bold = "\x1B[1m";
const std::string dim = "\x1B[2m";
const std::string under = "\x1B[4m";
const std::string reverse = "\x1B[7m";
const std::string hide = "\x1B[8m";

const std::string clearscreen = "\x1B[2J";
const std::string clearline = "\x1B[2K";

const std::string black = "\x1B[30m";
const std::string red = "\x1B[31m";
const std::string green = "\x1B[32m";
const std::string yellow = "\x1B[33m";
const std::string blue = "\x1B[34m";
const std::string magenta = "\x1B[35m";
const std::string cyan = "\x1B[36m";
const std::string white = "\x1B[37m";

const std::string bblack = "\x1B[90m";
const std::string bred = "\x1B[91m";
const std::string bgreen = "\x1B[92m";
const std::string byellow = "\x1B[93m";
const std::string bblue = "\x1B[94m";
const std::string bmagenta = "\x1B[95m";
const std::string bcyan = "\x1B[96m";
const std::string bwhite = "\x1B[97m";

const std::string newline = "\r\n\x1B[0m";

std::map<std::string, std::string> colorMap = { {"&k", black},
                                                {"&r", red},
                                                {"&g", green},
                                                {"&y", yellow},
                                                {"&b", blue},
                                                {"&m", magenta},
                                                {"&c", cyan},
                                                {"&w", white},
                                                {"&K", bblack},
                                                {"&R", bred},
                                                {"&G", bgreen},
                                                {"&Y", byellow},
                                                {"&B", bblue},
                                                {"&M", bmagenta},
                                                {"&C", bcyan},
                                                {"&W", bwhite},
                                                {"&x", reset} };


enum MineralTypes {
    BAUXITE = 1,
    MAGNETIDE = 2,
    QUARTZ = 3,
    GRAPHITE = 4,
    GUNDANIUM = 5
};

struct config {
    bool echo_on;
    bool color_on;
};

std::string tagsToEscapes(std::string input) {
    size_t found_offset = input.find('&');
    while (found_offset != input.npos && found_offset != -1) {
        std::string foundTag = std::format("{}{}", input.at(found_offset), input.at(found_offset + 1));
        if (colorMap.find(foundTag) != colorMap.end()) {
            input.erase(input.begin() + found_offset, input.begin() + found_offset + 2);
            input.insert(found_offset, colorMap[foundTag]);
        }
        else {
            input.erase(input.begin() + found_offset);
        }
        found_offset = input.find('&');
    }
    input.append(reset);
    return input;
}

std::string stripTags(std::string input) {
    size_t found_offset = input.find('&');
    while (found_offset != input.npos && found_offset != -1) {
        std::string foundTag = std::format("{}{}", input.at(found_offset), input.at(found_offset + 1));
        if (colorMap.find(foundTag) != colorMap.end()) {
            input.erase(input.begin() + found_offset, input.begin() + found_offset + 2);
        }
        else {
            input.erase(input.begin() + found_offset);
        }
        found_offset = input.find('&');
    }
    input.append(reset);
    return input;
}

class mudConn {
public:
    MudSocket sock;
    int state;
    char buffer[DEFAULT_BUFLEN];
    std::vector<std::string> cmdline;
    Player player;
    std::string args;
    config config;

    mudConn() {
        sock = MudSocket();
        state = -2;
        memset(buffer, ' ', sizeof(buffer));
        player = Player();
        args = "";
        config.echo_on = true;
        config.color_on = true;

    }

    mudConn(mudConn&& fromConn) {
        sock = fromConn.sock;
        state = fromConn.state;
        cmdline = fromConn.cmdline;
        memcpy_s(buffer, sizeof(buffer), fromConn.buffer, sizeof(fromConn.buffer));
        player = fromConn.player;
        args = fromConn.args;
        config.echo_on = fromConn.config.echo_on;
        config.color_on = fromConn.config.color_on;
        fromConn.sock.sock = INVALID_SOCKET;
    }

    void operator=(mudConn& fromConn) {
        sock = fromConn.sock;
        state = fromConn.state;
        cmdline = fromConn.cmdline;
        player = fromConn.player;
        args = fromConn.args;
        memcpy_s(buffer, sizeof(buffer), fromConn.buffer, sizeof(fromConn.buffer));
        config.echo_on = fromConn.config.echo_on;
        config.color_on = fromConn.config.color_on;
        fromConn.sock.sock = INVALID_SOCKET;
    }

    void operator=(mudConn&& fromConn) {
        sock = fromConn.sock;
        state = fromConn.state;
        cmdline = fromConn.cmdline;
        player = fromConn.player;
        args = fromConn.args;
        memcpy_s(buffer, sizeof(buffer), fromConn.buffer, sizeof(fromConn.buffer));
        config.echo_on = fromConn.config.echo_on;
        config.color_on = fromConn.config.color_on;
        fromConn.sock.sock = INVALID_SOCKET;
    }
};

void saveChar(mudConn& conn);

int send(mudConn& conn, const char* msg) {
    std::string escapedmsg = std::string(msg);

    if (conn.config.color_on)
        escapedmsg = tagsToEscapes(escapedmsg);
    else
        escapedmsg = stripTags(escapedmsg);

    return send(conn.sock.sock, escapedmsg.c_str(), strlen(escapedmsg.c_str()), 0);
}

int send(mudConn& conn, std::string msg) {
    return send(conn, msg.c_str());
}

int send(mudConn& conn, std::string msg, std::string color) {
    std::string totalMessage = color + msg + reset;
    return send(conn, totalMessage.c_str());
}

int send(mudConn *conn, std::string msg) {
    if (!conn) {
        printf("Bad pointer on connections!\n\r");
        return -1;
    }

    return send(*conn, msg.c_str());
}

struct Exit {
    size_t to_vnum;
    std::string name;
};

struct Room {
    size_t vnum;
    std::string name;
    std::string desc;
    std::vector<Exit> exits;
    std::vector<Player*> players;
};

Room& getRoomFromVnum(int vnum);
Room* getRoom(mudConn& conn);

struct Area {
    std::string name;
    std::string version;
    std::string author;
    std::string desc;
    int rVnumLow;
    int rVnumHi;
    int oVnumLow;
    int oVnumHi;
    int mVnumLow;
    int mVnumHi;
    std::vector<Room> rooms;
};

struct HelpEntry {
    std::string name;
    size_t level;
    std::string desc;
};

size_t getMaxLevel(levels& lvls) {
    size_t maxLvl = 0;
    maxLvl = max(maxLvl, lvls.combat);
    maxLvl = max(maxLvl, lvls.piloting);
    maxLvl = max(maxLvl, lvls.engineering);
    maxLvl = max(maxLvl, lvls.leadership);
    return maxLvl;
}

enum connStates {
    INIT = -2,
    GETNAME = -1,
    GETPASSWORD = 0,
    CONNECTED = 1,
    EDITING = 2,
    CLOSE = 3,
    REMOVE = 4,
    MAX_CONN_STATES
};

const char* WelcomeBanner()
{
    return "\x1B[2J\x1B[31mWelcome to MSMUD!\x1B[0m\n\r"
        "\x1B[32m                                    .-+*#*-\n\r"
        "                                  :-+=+#%%#*+\n\r"
        "                                 -=++*%%@@#+=-\n\r"
        "                                .*#*#%@@@@##+++\n\r"
        "                                 #%.....@@%#*+*              +#%@@@%#+++==\n\r"
        "             :*%%@@-            .%%.....#%%#++*-          =##%%%%%@@%@%#*++.\n\r"
        "          +##%%@@@@@@@%   -=++@@*+#=-=%#=+++*%@%#*=.     **++=*##@%@%@%%#*+=\n\r"
        "      =@@**+**#%@@@@@@@@#*%#*+@:::::%@@@@##@@#@@@@#+==-=*-.:-=+*#%%@@@@%%#+==.\n\r"
        "     :=+**=--+#%@@@@@@@@@@*+*%@#++*+#@@%@##%%@%@@@@@#==*-..::+#*##%@@%@@%#+===\n\r"
        "     -=:::::*%@@@@@@@@@@@@**#%%@#***#@@%@@%@@@%@@@@@@@@#+:::-=@**#%%@@%#*==@@\n\r"
        "     --::.:-*#@@@@@@@@@@+@=+%*=%@@%%%#+*#**##****@@@@@@%#+--+#++*%%@@@@@#**.\n\r"
        "     --:::-=*@@@@@@@@@*=====-=@#=====++++++++*##@@@@@@@@%#*--=*@@@@@@@@%##+\n\r"
        "     :----==#%@@@@@@@+=======@+====--=+++++++##@@@@@@@@@@%%%%@@@@@@@@@@%%+\n\r"
        "         +==*@@@@@@*:-------=@=--==+=-:::=##%%%@@@@@@@@@@@@@@@@@@@@@@@@%*-\n\r"
        "          -@@@@@@@@@++++=----@*-----:--=#%%%%%%@@@@@@@@@@@--+#@+. -*%@@@*::\n\r"
        "          ::..+%@@@@%+*****=-%@------*%%%%%%%%@@@@@@@@@%%+::: --::=*%@@@#:.\n\r"
        "          ==:-+#%@@@@##*****%%@*++**#@#@@@@@@@@@@@@@@@@+=:::: :+*==*#%@@@=.\n\r"
        "          .===*%@@@@@@-=+*+%@@@@%%%%@##@@@@@@@@@@@@@@@@@@%#-   -%@@@@@@@@@%\n\r"
        "           +@@@%@@@@@.    --:::::::*-%@@@@#%@@@@%#####+         +@@@@@-:::#**+-\n\r"
        "        ---#@@@@@@@@       =*###%%%@@@@@@#*#%@@%#***+             #@@@@..:#***+\n\r"
        "       :-=@@@@*::-%#        ----*%@@@@@@@@@@@@@###**           :--@@@##...#**++\n\r"
        "      ---%%@@#::-@%%#*:      +@@@#%%%@@@@@@@@@@@@@*            ---@@@%+:-*@@#*+\n\r"
        "--------------------------------------------------------------------------------\n\r\x1B[0m"
        "    Copyright(C) Tim Emmerich 2025. Inspired by SWReality 1.0 and GundamWingMUD.\n\r"
        "  Theme derived from Gundam-related properties owned by Bandai Namco Holdings. \n\r"
        "--------------------------------------------------------------------------------\n\r";
}

struct Fight {
    Player* fighter1;
    Player* fighter2;
    bool isOver;
};

bool isAlpha(std::string& str) {
    bool isAlpha = true;
    for (char currChar : str)
        if (!std::isalpha(currChar))
            isAlpha = false;

    return isAlpha;
}

bool isNumber(std::string& str) {
    bool isNumber = true;
    for (char currChar : str)
        if (!std::isdigit(currChar))
            isNumber = false;
    return isNumber;
}

const char* NamePrompt()
{
    return "Enter your name: ";
}

void movePlayerToRoom(mudConn& conn, Room& room);
bool isRoom(int vnum);

std::vector<mudConn> conns;
std::vector<Area> areas;
std::vector<Room> rooms;
std::vector<HelpEntry> helps;
std::vector<Player> mobs;
std::vector<Fight> fights;

void sendToAll(std::string msg, std::string color) {
    for (mudConn& conn : conns) {
        if (conn.state == connStates::CONNECTED) {
            send(conn, msg, color);
        }
    }
}

void do_quit(mudConn& conn) {
    saveChar(conn);
    std::string msg ="&C[&WINFO&c]&W " + conn.player.name + " &whas left MSMUD!\n\r";

    for (int i = 0; i < conns.size(); ++i)
    {
        send(conns[i], msg);
    }
    conn.state = connStates::CLOSE;
    std::erase_if(getRoom(conn)->players, [&conn](Player* player) { return player->name == conn.player.name; });
}

void do_who(mudConn &conn){
    std::string msg;
    send(conn, std::format("&c[[&w=-------------------------====&C([&r{} &w{}&C]))&w====-------------------------=&c]]&x\n\r", MSMUD::mudName, MSMUD::version));
    for (auto &currConn : conns)
    {
        send(conn, std::format("&W[&w{:>3}&W] &w{}\n\r", getMaxLevel(currConn.player.levels), currConn.player.title));
    }
}

void sendPrompt(mudConn& conn) {
    std::string hpcolor = "&G";
    double hpfraction = (double)conn.player.hp / (double)conn.player.maxhp;
    if (hpfraction < 0.85)
        hpcolor = "&g";
    if (hpfraction < 0.7)
        hpcolor = "&y";
    if (hpfraction < 0.4) 
        hpcolor = "&Y";
    if (hpfraction < 0.25)
        hpcolor = "&R";
    if (hpfraction < 0.15)
        hpcolor = "&r";
    if (hpfraction > 1)
        hpcolor = "&C";
    
    if (conn.state == connStates::CONNECTED) {
        send(conn, std::format("&w({}{}&w/{}{}&w)hp &w({})foc>&x ", hpcolor, conn.player.hp, hpcolor, conn.player.maxhp, conn.player.focus));
    }
    else {
        send(conn, "> ");
    }
}

void do_ooc(mudConn& conn) {
    std::string msg;
    if (conn.args == "") {
        msg = "OOC what?\n\r";
        send(conn, msg);
    }
    else
    {
        msg = "[OOC] " + conn.player.name + ": '" + conn.args + "'\n\r";
        for (auto& currConn : conns) {
            send(currConn, msg);
        }
    }
}

bool directoryExists(const std::string& dirPath) {
    DWORD fileAttributes = GetFileAttributes(dirPath.c_str());
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
        return false; // Path does not exist
    }
    return (fileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool loadArea(std::string filename) {

    enum readState {
        HEADER = 1,
        ROOM = 2,
        ITEM = 3,
        MOB = 4,
        NEXT = 5
    };

    int state = HEADER;
    std::string subdir = AREAPATH;

    std::ifstream inFile(subdir + filename);

    printf("Loading area '%s'...\n\r", filename.c_str());

    if (!inFile) {
        printf("Couldn't find area file '%s' in areas folder '%s'.\n\r", filename.c_str(), subdir.c_str());
        return false;
    }

    Area newArea;
    Room* currentRoom;

    std::string line;
    while (std::getline(inFile, line)) {

        if (line == "") {
            state = NEXT;
        }

        else if (line.substr(0, 8) == "[area : ") {
            state = HEADER;
            line = line.substr(8, line.size() - 8);
            line.pop_back();
            newArea.name = line;
        }
        else if (line.substr(0, 8) == "[room : ") {
            state = ROOM;
            line = line.substr(8, line.size() - 8);
            if (atoi(line.c_str()) < 0) {
                printf("invalid input for room vnum while reading area file. aborting.\n\r");
                return false;
            }
            Room newRoom;
            newArea.rooms.push_back(newRoom);
            currentRoom = &newArea.rooms.back();
            currentRoom->vnum = atoi(line.c_str());

        }
        else if (line.substr(0, 12) == "- version : ") {
            line = line.substr(12, line.size() - 12);
            newArea.version = line;
        }
        else if (line.substr(0, 11) == "- author : ") {
            line = line.substr(11, line.size() - 11);
            newArea.author = line;
        }
        else if (line.substr(0, 13) == "- rvnumlow : ") {
            line = line.substr(13, line.size() - 13);
            if (atoi(line.c_str()) < 0) {
                printf("invalid number provided for rvnumlow. Aborting.\n\r");
                return false;
            }
            newArea.rVnumLow = atoi(line.c_str());
        }
        else if (line.substr(0, 12) == "- rvnumhi : ") {
            line = line.substr(12, line.size() - 12);
            if (atoi(line.c_str()) < 0) {
                printf("invalid number provided for rvnumhi. Aborting.\n\r");
                return false;
            }
            newArea.rVnumHi = atoi(line.c_str());
        }
        else if (line.substr(0, 13) == "- ovnumlow : ") {
            line = line.substr(13, line.size() - 13);
            if (atoi(line.c_str()) < 0) {
                printf("invalid number provided for ovnumlow. Aborting.\n\r");
                return false;
            }
            newArea.oVnumLow = atoi(line.c_str());
        }
        else if (line.substr(0, 12) == "- ovnumhi : ") {
            line = line.substr(12, line.size() - 12);
            if (atoi(line.c_str()) < 0) {
                printf("invalid number provided for ovnumhi. Aborting.\n\r");
                return false;
            }
            newArea.oVnumHi = atoi(line.c_str());
        }
        else if (line.substr(0, 13) == "- mvnumlow : ") {
            line = line.substr(13, line.size() - 13);
            if (atoi(line.c_str()) < 0) {
                printf("invalid number provided for mvnumlow. Aborting.\n\r");
                return false;
            }
            newArea.mVnumLow = atoi(line.c_str());
        }
        else if (line.substr(0, 12) == "- mvnumhi : ") {
            line = line.substr(12, line.size() - 12);
            if (atoi(line.c_str()) < 0) {
                printf("invalid number provided for mvnumhi. Aborting.\n\r");
                return false;
            }
            newArea.mVnumHi = atoi(line.c_str());
        }
        else if (line.substr(0, 9) == "- desc : ") {
            line = line.substr(9, line.size() - 9);
            if (line.back() == '~') {
                line.pop_back();
                switch (state) {
                    case HEADER:
                         newArea.desc = line;
                    break;

                    case ROOM:
                        currentRoom->desc = line;
                    break;
                }
            }
            else
            {
                printf("description without closing tilde. aborting!\n\r");
                return false;
            }
        }
        else if (line.substr(0, 9) == "- exit : ") {
            line = line.substr(9, line.size() - 9);
            Exit newExit;
            newExit.name = line.substr(0, line.find(','));
            line = line.substr(line.find(',') +2, line.size() - line.find(',')-2);
            if (atoi(line.c_str()) < 0) {
                printf("invalid vnum provided for exit. aborting.\n\r");
                return false;
            }
            newExit.to_vnum = atoi(line.c_str());
            currentRoom->exits.push_back(newExit);
        }
        else if (line.substr(0, 9) == "- name : ") {
            line = line.substr(9, line.size() - 9);
            switch (state) {
                case ROOM:
                    currentRoom->name = line;
                break;

            }
        }
 
    }

    for (auto &room : newArea.rooms) {
        rooms.push_back(room);
    }
}

std::string getWord(std::string input) {
    std::string word = { "" };
    size_t offset = input.find('0');
    if (offset == input.npos) {
        return word;
    }
    else {
        word = input.substr(0, offset);
        return word;
    }
}

std::string removeWhiteSpace(std::string input) {
    input.erase(std::remove_if(input.begin(), input.end(), ::isspace), input.end());
    return input;
}

bool loadAreas() {
    std::string subdir = AREAPATH;
    std::string filename = AREALISTFILE;

    std::ifstream inFile(subdir + filename);

    printf("Loading areas...\n\r");

    if (!inFile) {
        printf("Couldn't find area list at path '%s' with filename '%s.'\n\r", subdir.c_str(), filename.c_str());
        return false;
    }

    std::string line;
    while (std::getline(inFile, line)) {
        loadArea(line);
    }
}

bool loadHelps() {
    std::string subdir = HELPPATH;

    printf("Loading help entries...\n\r");

    for (auto& dirEntry : std::filesystem::recursive_directory_iterator(std::filesystem::path(subdir))) {
        HelpEntry newHelp;
        std::ifstream inFile(dirEntry.path().string());
        std::string line;

        if (!inFile) {
            printf("Couldn't read help file '%s'.\n\r", dirEntry.path().string().c_str());
            return false;
        }
        std::getline(inFile, line);
        if (line.substr(0, 9) == "- name : ") {
            line = line.substr(9, line.size() - 9);
            newHelp.name = line;
        }

        std::getline(inFile, line);
        if (line.substr(0, 10) == "- level : ") {
            line = line.substr(10, line.size() - 10);
            newHelp.level = std::stoi(line);
        }

        std::getline(inFile, line);
        if (line.substr(0, 9) == "- desc : ") {
            line = line.substr(9, line.size() - 9);
            newHelp.desc = line;
        }
        helps.push_back(newHelp);
    }
    return true;
}

void do_score(mudConn& conn) {
    std::string msg;
    msg = std::format("&c[[&w=-------------------------====&C([&r{} &w{}&C]))&w====---------------=&C([&wScore&C]))&c]]\n\r", MSMUD::mudName, MSMUD::version);
    msg += "&cName: &w" + conn.player.name + "\n\r";
    msg += "&cTitle: &x" + conn.player.title + "\n\r";
    msg += "&cStats:      ";
    msg += "&cStr: " + std::format("&w{:2}", conn.player.stats.strength) + " ";
    msg += "&cInt: " + std::format("&w{:2}", conn.player.stats.intellect) + " ";
    msg += "&cDex: " + std::format("&w{:2}", conn.player.stats.dexterity) + " ";
    msg += "&cCon: " + std::format("&w{:2}", conn.player.stats.constitution) + "\n\r";
    msg += "&cLevels:     ";
    msg += "&cCombat:      " + std::format("&w{:3} ({:8}/{:8})\n\r", conn.player.levels.combat, conn.player.xp.combat, XPToNextLevel(conn.player.levels.combat));
    msg += "            &cPiloting:    " + std::format("&w{:3} ({:8}/{:8})\n\r", conn.player.levels.piloting, conn.player.xp.piloting, XPToNextLevel(conn.player.levels.piloting));
    msg += "            &cEngineering: " + std::format("&w{:3} ({:8}/{:8})\n\r", conn.player.levels.engineering, conn.player.xp.engineering, XPToNextLevel(conn.player.levels.engineering));
    msg += "            &cLeadership:  " + std::format("&w{:3} ({:8}/{:8})\n\r", conn.player.levels.leadership, conn.player.xp.leadership, XPToNextLevel(conn.player.levels.leadership));
    msg += "&w-------------------------------------------------------------------------------\n\r";
    msg += "&c[[&w---------------------------------------------------------------------------&c]]\n\r";
    msg += "&wAffected by: None.\n\r\n\r";
    send(conn, msg);
}

void saveChar(mudConn& conn) {
    std::string subdir = "./players/";
    subdir.append(conn.player.name.substr(0, 1));

    if (!directoryExists(subdir))
        CreateDirectory(subdir.c_str(), NULL);

    std::string filename = subdir;
    filename.append("/");
    filename.append(conn.player.name);
    filename.append(".txt");
    std::ofstream outFile(filename);

    if (!outFile) {
        printf("Error opening character file '%s' in directory '%s' for write.\n\r", filename.c_str(), subdir.c_str());
        return;
    }

    outFile << "Name: " << conn.player.name << "\n";
    outFile << "Password: " << conn.player.password << "\n";
    outFile << "Title: " << conn.player.title << "\n";
    outFile << "Stats: " << conn.player.stats.strength << " " << conn.player.stats.intellect;
    outFile << " " << conn.player.stats.dexterity << " " << conn.player.stats.constitution << "\n";
    outFile << "Levels: " << conn.player.levels.combat << " " << conn.player.levels.piloting;
    outFile << " " << conn.player.levels.engineering << " " << conn.player.levels.leadership << "\n";
    outFile << "XP: " << conn.player.xp.combat << " " << conn.player.xp.piloting;
    outFile << " " << conn.player.xp.engineering << " " << conn.player.xp.leadership << "\n";
    outFile << "RoomVnum: " << conn.player.roomVnum << "\n";
    outFile << "HP: " << conn.player.hp << "\n";
    outFile << "MaxHP: " << conn.player.maxhp << "\n";
    outFile << "Focus: " << conn.player.focus << "\n";
    outFile.close();

   // conn.player.lastTimeSaved = time(nullptr);
}
void do_save(mudConn& conn) {
    saveChar(conn);
    send(conn, "Ok.\n\r");
}

void loadChar(mudConn& conn) {
    std::string subdir = "./players/";
    subdir.append(conn.player.name.substr(0, 1));

    if (!directoryExists(subdir))
        return;

    std::string filename = subdir;
    filename.append("/");
    filename.append(conn.player.name);
    filename.append(".txt");

    std::ifstream inFile(filename);

    if (!inFile)
        return;

    std::string line;
    std::getline(inFile, line); //reads name line into line
    std::getline(inFile, line); //reads password line into line

    one_argument(line);
    conn.player.password = line;

    std::getline(inFile, line); //reads title line into line
    one_argument(line); //drop 'Title: '
    conn.player.title = line;

    std::getline(inFile, line); //reads stats line into line
    one_argument(line); //drop 'stats: '
    conn.player.stats.strength = std::stoi(one_argument(line));
    conn.player.stats.intellect = std::stoi(one_argument(line));
    conn.player.stats.dexterity = std::stoi(one_argument(line));
    conn.player.stats.constitution = std::stoi(line);

    std::getline(inFile, line); //reads levels line into line
    one_argument(line); //drop 'levels: '
    conn.player.levels.combat = std::stoi(one_argument(line));
    conn.player.levels.piloting = std::stoi(one_argument(line));
    conn.player.levels.engineering = std::stoi(one_argument(line));
    conn.player.levels.leadership = std::stoi(line);

    std::getline(inFile, line); //reads xp line into line;
    one_argument(line); //drops 'xp: '
    conn.player.xp.combat = std::stoi(one_argument(line));
    conn.player.xp.piloting = std::stoi(one_argument(line));
    conn.player.xp.engineering = std::stoi(one_argument(line));
    conn.player.xp.leadership = std::stoi(line);

    std::getline(inFile, line); // reads room vnum line into line
    one_argument(line); //drop 'RoomVnum: '
    conn.player.roomVnum = std::stoi(line);

    std::getline(inFile, line); //reads hp line into line
    one_argument(line); //drop 'HP: '
    conn.player.hp = std::stoi(line);

    std::getline(inFile, line); //reads maxhp line into line
    one_argument(line); //drop 'MaxHP: '
    conn.player.maxhp = std::stoi(line);

    std::getline(inFile, line); //reads focus line into line
    one_argument(line); //drop 'Focus: '
    conn.player.focus = std::stoi(line);

    inFile.close();

    return;
}

std::string one_argument(std::string& args) {
    std::string argument = "";
    auto space_iter = std::find(args.begin(), args.end(), ' ');
    if (space_iter != args.end()) {
        auto offset = std::distance(args.begin(), space_iter);
        argument = args.substr(0, offset);
        args = args.substr(offset + 1, args.size() - offset);
        return argument;
    }
    return "";
}

void do_password(mudConn& conn) {
    if (conn.args == "")
        send(conn, "Usage: password oldpassword newpassword newpassword\n\r");
    else
    {
        std::string oldPass = one_argument(conn.args);
        std::string newPass1 = one_argument(conn.args);
        std::string newPass2 = conn.args;
        if ((oldPass == "" ) || (newPass1 == "") || (newPass2 == "")) {
            send(conn, "Usage: password oldpassword newpassword newpassword\n\r");
            return;
        }

        if (oldPass != conn.player.password) {
            send(conn, "Incorrect password provided as old password. Provide the correct current password.\n\r");
            return;
        }

        if (newPass1 != newPass2) {
            send(conn, "New passwords don't match. Please try again.\n\r");
            return;
        }
       
        conn.player.password = newPass1;
        send(conn, "New password set.\n\r");
        saveChar(conn);

        return;
    }
}

Room *getRoom(mudConn& conn) {
    Room *room = nullptr;
    for (auto& currRoom : rooms) {
        if (currRoom.vnum == conn.player.roomVnum)
            room = &currRoom;
    }
    return room;
}

Room* getRoom(mudConn *conn)
{
    Room* room = nullptr;
    for (auto& currRoom : rooms) {
        if (currRoom.vnum == conn->player.roomVnum)
            room = &currRoom;
    }
    return room;
}

void do_look(mudConn& conn) {
    Room* room = getRoom(conn);
    if (!room) {
        printf("Error, character has no room?! Fixing room.\n\r");
        conn.player.roomVnum = STARTING_ROOM;
        return;
    }

    if (conn.player.state < RESTING) {
        send(conn, "In your dreams, or what?\n\r");
        return;
    }

    send(conn, std::format("&C(&W{}&C)\n\r", room->name));
    send(conn, std::format("&w{}\n\r\n\r", room->desc));

    std::string msg = "&W";

    if (room->exits.size() > 0) {
        send(conn, "&wExits: \n\r");
        for (int i = 0; i < room->exits.size(); i++) {
            msg.append(room->exits[i].name);
            if (i != room->exits.size() - 1)
                msg.append(", ");
        }
        msg.append("\n\r");
        send(conn, msg);
    }
    else {
        send(conn, std::string("There are no obvious exits.\n\r"), bwhite);
    }

    if (room->players.size() > 0) {
        for (Player* player : room->players) {
            if (!player) break;
            if (player->name != conn.player.name) {
                std::string playerPosition = "";

                if (player->state == SITTING) {
                    playerPosition = "sitting";
                }
                else if (player->state == SLEEPING) {
                    playerPosition = "sleeping";
                }
                else if (player->state == STANDING) {
                    playerPosition = "standing";
                }
                else if (player->state == FIGHTING) {
                    playerPosition = "in a fight";
                }

                send(conn, std::format("&y{} is {} here.\n\r", player->name, playerPosition));
            }
        }
    }

    send(conn, "\n\r");
    return;
}

void do_kill(mudConn& conn) {

    Player* target = nullptr;
    if (conn.args.size() > 0) {
        if (conn.args == conn.player.name) {
            send(conn, "You don't actually wanna do it...\n\r");
            return;
        }

        for (Player* player : getRoom(conn)->players) {
            if (player->name == conn.args) {
                target = player;
            }
        }
        if (target == nullptr) {
            send(conn, "You do not see them here.\n\r");
            return;
        }
        else {
            conn.player.state = FIGHTING;
            target->state = FIGHTING;
            fights.push_back(Fight{ &conn.player, target, false });
        }
    }
    else {
        send(conn, "Kill who?\n\r");
    }
}

void do_time(mudConn& conn) {
    const auto tp{ std::chrono::system_clock::now() };
    auto timeString = std::chrono::current_zone()->to_local(tp);
    std::string msg = std::format("The MUD time is currently {}.\n\r", timeString);
    send(conn, msg);
}

void do_title(mudConn& conn) {
    if (conn.args == "") {
        send(conn, "Change your title to what?\n\r");
        return;
    }

    if (!conn.args.contains(conn.player.name)) {
        send(conn, "Your title must contain your name.\n\r");
        return;
    }
    else {
        conn.player.title = conn.args;
        send(conn, "Ok.\n\r");
    }
}

bool HelpExists(std::string name) {
    for (HelpEntry& help : helps) {
        if (help.name == name)
            return true;
    }
    return false;
}

HelpEntry& getHelpEntry(std::string name) {
    for (HelpEntry& help : helps) {
        if (help.name == name)
            return help;
    }
}

void do_help(mudConn& conn) {
    if (conn.args == "") {
        send(conn, "Usage: help topic\n\r");
        return;
    }
    else
    {
        if (!HelpExists(conn.args)) {
            send(conn, std::format("No help found on topic '{}'.\n\r", conn.args));
            return;
        }
        else
        {
            HelpEntry help = getHelpEntry(conn.args);
            if (help.level <= getMaxLevel(conn.player.levels)) {
                send(conn, std::format("Name: {}\n\rLevel: {}\n\r{}\n\r", help.name, help.level, help.desc));
            }
            else {
                send(conn, std::format("No help found on topic '{}'.\n\r", conn.args));
                return;
            }
        }
    }
}

bool isImm(Player& player) {
    if (getMaxLevel(player.levels) >= LEVEL_GOD)
        return true;
    else
        return false;
}

void do_goto(mudConn& conn) {
    if (!isImm(conn.player)) {
        send(conn, "Huh?\n\r", white);
        return;
    }

    if (conn.args == "") {
        send(conn, "Syntax: goto rvnum\n\r", white);
        return;
    }

    if (!isNumber(conn.args)) {
        send(conn, "Invalid rvnum provided.\n\r", yellow);
        return;
    }

    int rVnum = std::stoi(conn.args);

    if (!isRoom(rVnum)) {
        send(conn, std::format("No room with vnum '{}' found.\n\r", rVnum), yellow);
        return;
    }
    else {
        movePlayerToRoom(conn, getRoomFromVnum(rVnum));
        return;
    }
}

mudConn& getConnFromPlayer(Player& player) {
    for (mudConn& conn : conns) {
        if (conn.player.name == player.name)
            return conn;
    }
}

mudConn* getConnFromPlayer(Player* player) {
    mudConn* foundConn = nullptr;
    for (mudConn& conn : conns) {
        if (conn.player.name == player->name)
            foundConn = &conn;
    }
    return foundConn;
}

void do_say(mudConn& conn) {

    if (conn.args == "") {
        send(conn, "Say what?\n\r");
        return;
    }

    std::string msg = conn.args;
    send(conn, std::format("You say '{}.'\n\r", msg));

    for (auto& player : getRoom(conn)->players) {
        if (player->state != SLEEPING && player->name != conn.player.name) {
            send(getConnFromPlayer(player), std::format("{} says '{}.'\n\r", conn.player.name, msg));
        }
    }
}

void do_color(mudConn& conn) {
    if (conn.config.color_on) {
        conn.config.color_on = false;
        send(conn, "Color now off.\n\r");
        return;
    }
    else {
        conn.config.color_on = true;
        send(conn, "Color now on.\n\r");
        return;
    }
}

void do_hp(mudConn& conn) {
    if (!isImm(conn.player)) {
        send(conn, "Huh?\n\r");
        return;
    }

    if (conn.args == "") {
        send(conn, "Set your hp to what?\n\r");
        return;
    }

    if (!isNumber(conn.args)) {
        send(conn, "Invalid number provided.\n\r");
        return;
    }
    else {
        conn.player.hp = std::stoi(conn.args);
        send(conn, std::format("HP set to {}.\n\r", conn.player.hp));
        return;
    }
}

void do_sit(mudConn& conn) {
    std::string msg;
    switch (conn.player.state) {
    case SITTING:
        send(conn, "You are already sitting.\n\r");
        break;

    case SLEEPING:
        send(conn, "You wake and sit up.\n\r");
        conn.player.state = SITTING;
        msg = std::format("{} opens their eyes and sits up.\n\r", conn.player.name);
        for (auto& player : getRoom(conn)->players) {
            if (player->name != conn.player.name) {
                send(getConnFromPlayer(player), msg);
            }
        }
        break;

    case STANDING:
        send(conn, "You sit down.\n\r");
        conn.player.state = SITTING;
        msg = std::format("{} sits down.\n\r", conn.player.name);
        for (auto& player : getRoom(conn)->players) {
            if (player->name != conn.player.name) {
                send(getConnFromPlayer(player), msg);
            }
        }
        break;
    }
}

void do_sleep(mudConn& conn) {
    std::string msg;
    switch (conn.player.state) {
    case SITTING:
        send(conn, "You slump over into a deep sleep.\n\r");
        conn.player.state = SLEEPING;
        msg = std::format("{} slumps over into a deep sleep.\n\r", conn.player.name);
        for (auto& player : getRoom(conn)->players) {
            if (player->name != conn.player.name) {
                send(getConnFromPlayer(player), msg);
            }
        }
        break;

    case SLEEPING:
        send(conn, "You try to dream even harder.\n\r");
        break;

    case STANDING:
        send(conn, "You collapse into a deep sleep. Luckily you didn't take damage!\n\r");
        conn.player.state = SLEEPING;
        msg = std::format("{} collapses into a deep sleep, but they seem to be okay.\n\r", conn.player.name);
        for (auto& player : getRoom(conn)->players) {
            if (player->name != conn.player.name) {
                send(getConnFromPlayer(player), msg);
            }
        }
        break;
    }
}

void do_stand(mudConn& conn) {
    std::string msg;
    switch (conn.player.state) {
    case SITTING:
        send(conn, "You stop sitting and stand.\n\r");
        conn.player.state = STANDING;
        msg = std::format("{} stops sitting and stands up.\n\r", conn.player.name);
        for (auto& player : getRoom(conn)->players) {
            if (player->name != conn.player.name) {
                send(getConnFromPlayer(player), msg);
            }
        }
        break;

    case SLEEPING:
        send(conn, "You wake up and stand.\n\r");
        conn.player.state = STANDING;
        msg = std::format("{} wakes and stands up.\n\r", conn.player.name);
        for (auto& player : getRoom(conn)->players) {
            if (player->name != conn.player.name) {
                send(getConnFromPlayer(player), msg);
            }
        }
        break;

    case STANDING:
        send(conn, "You're already standing.\n\r");
        break;
    }
}


std::map<std::string, std::function<void(mudConn& conn)>> cmdList = {
    {"color", do_color},
    {"goto", do_goto},
    {"help", do_help},
    {"hp", do_hp},
    {"kill", do_kill}, 
    {"look", do_look},
    {"ooc", do_ooc},
    {"password", do_password},
    {"quit", do_quit},
    {"save", do_save},
    {"say", do_say},
    {"score", do_score},
    {"sit", do_sit},
    {"sleep", do_sleep},
    {"stand", do_stand},
    {"time", do_time},
    {"title", do_title},
    {"who", do_who} };

void trimTrailingWhitespace(std::string& str) {
    str.erase(std::find_if(str.rbegin(),
        str.rend(),
        [](unsigned char ch) { return !std::isspace(ch); }).base(), str.end());
}

std::string getPassword(mudConn& conn) {
    std::string subdir = "./players/";
    subdir.append(conn.player.name.substr(0, 1));

    if (!directoryExists(subdir))
        return "No Character exists.";

    std::string filename = subdir;
    filename.append("/");
    filename.append(conn.player.name);
    filename.append(".txt");

    std::ifstream inFile(filename);

    if (!inFile)
        return "No Character exists.";

    std::string line;
    std::getline(inFile, line); //load name line and skip it
    std::getline(inFile, line); //load password line and skip it

    line = line.substr(std::distance(line.begin(), std::find(line.begin(), line.end(), ':')+2), line.size() - std::distance(line.begin(), std::find(line.begin(), line.end(), ':'))-1); // read password into line

    inFile.close();
    
    return line;
}

bool checkForExit(mudConn& conn, std::string cmd) {
    std::string lowercmd = std::string(cmd.size(), 'X');
    std::transform(cmd.begin(), cmd.end(), lowercmd.begin(), [](unsigned char c) {return std::tolower(c); });
    std::string lowerexitname = "2";
    Room* currRoom = getRoom(conn);
    for (Exit& exit : currRoom->exits) {
        lowerexitname = std::string(exit.name.size(), 'X');
        std::transform(exit.name.begin(), exit.name.end(), lowerexitname.begin(), [](unsigned char c) {return std::tolower(c); });
        if (lowerexitname == lowercmd) {
            return true;
        }
    }
    return false;
}

bool isRoom(int vnum) {
    for (Room& room : rooms) {
        if (room.vnum == vnum) return true;
    }
    return false;
}

Room &getRoomFromVnum(int vnum) {
    for (Room& room : rooms) {
        if (room.vnum == vnum) return room;
    }
}

void movePlayerToRoom(mudConn& conn, Room& room) {
    if (getRoom(conn) == nullptr) {

    }
    std::erase_if(getRoom(conn)->players, [&conn](Player* currPlayer) {return (conn.player.name == currPlayer->name); });
    for (Player* player : getRoom(conn)->players) {
        send(getConnFromPlayer(player), std::format("{} leaves.\n\r", conn.player.name));
    }
    conn.player.roomVnum = room.vnum;
    for (Player* player : room.players) {
        send(getConnFromPlayer(player), std::format("{} has arrived.\n\r", conn.player.name));
    }

    room.players.push_back(&conn.player);
    do_look(conn);
}

void processCmdLine(std::string cmdline, mudConn& conn) {
    std::string msg;
    std::string cmd;

    auto space_iter = std::find(cmdline.begin(), cmdline.end(), ' ');

    if (space_iter != cmdline.end()) {
        int space_pos = std::distance(cmdline.begin(), space_iter);
        conn.args = cmdline.substr(space_pos + 1, cmdline.size() - (space_pos + 1));
        trimTrailingWhitespace(conn.args);
        cmd = cmdline.substr(0, space_pos);
    }
    else {
        conn.args = "";
        cmd = cmdline;
    }

    switch (conn.state) {
    case GETNAME:
        if (cmd.size() < 2) {
            send(conn, "Names must be at least two alphabetical characters. Sorry!\n\r");
            conn.player.name = "";
            return;
        }
        if (!isAlpha(cmd)) {
            send(conn, "Names must only contain alphabetical characters. Sorry!\n\r");
            conn.player.name = "";
            return;
        }
        conn.player.name = cmd;

        conn.state = connStates::GETPASSWORD;
        break;
    case GETPASSWORD:
        if (getPassword(conn) == "No Character exists.") {
            send(conn, "New player detected. Welcome!\n\r");
            send(conn, "Enter a password: ");

            conn.player.title = conn.player.name + " the most welcome newbie";
            conn.player.password = cmd;
            conn.player.roomVnum = STARTING_ROOM;
            getRoom(conn)->players.push_back(&conn.player);
            conn.state = connStates::CONNECTED;
        }
        else if (getPassword(conn) == cmd) {
            loadChar(conn);
            msg = "Welcome " + conn.player.name + "!\n\r";
            Room* room = getRoom(conn);
            if (!room) {
                conn.player.roomVnum = STARTING_ROOM;
                room = getRoom(conn);
            }
            room->players.push_back(&conn.player);
            send(conn, msg);
            conn.state = connStates::CONNECTED;
        }
        else {
            msg = "Incorrect password.\n\r";
            send(conn, msg);
            conn.player.name = "";
            conn.player.password = "";
            conn.state = connStates::GETNAME;
        }
        break;
    case CONNECTED:
        if (cmdList.find(cmd) != cmdList.end())
            cmdList[cmd](conn);
        else if (cmd == "") {
            send(conn, "");
        }
        else if (checkForExit(conn, cmd)) {
            std::string lowercmd = std::string(cmd.size(), 'X');
            std::string lowerexitname;
            std::transform(cmd.begin(), cmd.end(), lowercmd.begin(), [](unsigned char c) {return std::tolower(c); });
            for (Exit &exit : getRoom(conn)->exits) {
                lowerexitname = std::string(exit.name.size(), 'X');
                std::transform(exit.name.begin(), exit.name.end(), lowerexitname.begin(), [](unsigned char c) { return std::tolower(c); });
                if (lowerexitname == lowercmd)
                {
                    movePlayerToRoom(conn, getRoomFromVnum(exit.to_vnum));
                }
            }
        }
        else {
            send(conn, "Huh?\n\r");
        }
        sendPrompt(conn);
        break;
    }
}

void bufferToCmdLine(mudConn &conn, size_t charCount) {
    std::string receivedChars = "";
    for (int i = 0; i < charCount; ++i) {
        receivedChars = receivedChars + conn.buffer[i];
    }
    if (conn.cmdline.size() < 1)
        conn.cmdline.push_back(receivedChars);
    else {
        conn.cmdline[0].append(receivedChars);

        int enters = std::count_if(conn.cmdline[0].begin(), conn.cmdline[0].end(), [](const char input) {return input == '\n' || input == ';'; });
        for (; enters > 0; --enters) {
            auto pos_iter = std::find(conn.cmdline[0].begin(), conn.cmdline[0].end(), '\n');
            if (pos_iter == conn.cmdline[0].end())
                break;
            int pos_int = std::distance(conn.cmdline[0].begin(), pos_iter);
            std::string rest = conn.cmdline[0].substr(pos_int + 1, conn.cmdline[0].size() - pos_int);
            std::string clipped_line = conn.cmdline[0].substr(0, pos_int - 1);
            conn.cmdline[0] = rest;
            conn.cmdline.insert(conn.cmdline.begin() + 1, clipped_line);
        }
    }

        memcpy(conn.buffer, &conn.buffer[charCount], sizeof(conn.buffer));
}

void hitGain() {
    for (mudConn& conn : conns) {
        if (conn.state == connStates::CONNECTED) {
            if (conn.player.hp != conn.player.maxhp && conn.player.state != FIGHTING) {
                size_t gain = (conn.player.stats.constitution * 2) + (size_t)((double)std::rand() / RAND_MAX * conn.player.stats.constitution/2) - (size_t)((double)std::rand() / RAND_MAX * conn.player.stats.constitution/2);
                if (conn.player.state == STANDING) gain /= 4;
                else if (conn.player.state == SITTING) gain /= 2;
                printf((std::to_string(gain) + "\n\r").c_str());
                size_t missingHealth = conn.player.maxhp - conn.player.hp;
                gain = min(gain, missingHealth);
                send(conn, std::format("&WYou feel your wounds healing. &w(&G+{}hp&w)\n\r", gain));
                conn.player.hp += gain;
                if (conn.player.hp == conn.player.maxhp) {
                    send(conn, "You're back tip-top shape.\n\r");
                }
                sendPrompt(conn);
            }
        }
    }
}

void fightRound() {
    for (Fight& fight : fights) {
        if (fight.fighter1 == nullptr || fight.fighter2 == nullptr) {
            fight.isOver = true;
            break;
        }
        int dmg = fight.fighter1->stats.strength + (int)((double)std::rand() / (double)RAND_MAX * 5);
        send(getConnFromPlayer(fight.fighter1), std::format("You hit {} for {}.\n\r", fight.fighter2->name, dmg));
        send(getConnFromPlayer(fight.fighter2), std::format("{} hits you for {}.\n\r", fight.fighter1->name, dmg));

        for (Player* player : getRoom(getConnFromPlayer(fight.fighter1))->players) {
            if (player->name != fight.fighter1->name && player->name != fight.fighter2->name) {
                send(getConnFromPlayer(player), std::format("{} hits {} for {}.\n\r", fight.fighter1->name, fight.fighter2->name, dmg));
            }
        }
        fight.fighter2->hp -= dmg;

        dmg = fight.fighter2->stats.strength + (int)((double)std::rand() / (double)RAND_MAX * 5);
        send(getConnFromPlayer(fight.fighter2), std::format("You hit {} for {}.\n\r", fight.fighter1->name, dmg));
        send(getConnFromPlayer(fight.fighter1), std::format("{} hits you for {}.\n\r", fight.fighter2->name, dmg));


        for (Player* player : getRoom(getConnFromPlayer(fight.fighter2))->players) {
            if (player->name != fight.fighter1->name && player->name != fight.fighter2->name) {
                send(getConnFromPlayer(player), std::format("{} hits {} for {}.\n\r", fight.fighter2->name, fight.fighter1->name, dmg));
                sendPrompt(*getConnFromPlayer(player));
            }
        }

        sendPrompt(*getConnFromPlayer(fight.fighter1));
        sendPrompt(*getConnFromPlayer(fight.fighter2));

        fight.fighter1->hp -= dmg;

        if (fight.fighter1->hp <= 0) {
            fight.isOver = true;
            fight.fighter1->hp = 10;
        }

        if (fight.fighter2->hp <= 0) {
            fight.isOver = true;
            fight.fighter2->hp = 10;
        }

       

        if (fight.isOver == true) {

            send(getConnFromPlayer(fight.fighter1), "The fight has ended.\n\r");
            send(getConnFromPlayer(fight.fighter2), "The fight has ended.\n\r");

            for (Player* player : getRoom(getConnFromPlayer(fight.fighter1))->players) {
                if (player->name != fight.fighter1->name && player->name != fight.fighter2->name) {
                    send(getConnFromPlayer(player), std::format("The fight between {} and {} has ended.\n\r", fight.fighter1->name, fight.fighter2->name));
                    sendPrompt(*getConnFromPlayer(player));
                }
            }

            fight.fighter1->state = STANDING;
            fight.fighter2->state = STANDING;

            size_t xpGainF1 = (size_t)((double)getMaxLevel(fight.fighter2->levels) / (double)getMaxLevel(fight.fighter1->levels) * (double)XPToNextLevel(getMaxLevel(fight.fighter1->levels)) / 5);
            size_t xpGainF2 = (size_t)((double)getMaxLevel(fight.fighter1->levels) / (double)getMaxLevel(fight.fighter2->levels) * (double)XPToNextLevel(getMaxLevel(fight.fighter2->levels)) / 5);

            if (fight.fighter1->levels.combat != maxLevel) {
                fight.fighter1->xp.combat += xpGainF1;
                send(getConnFromPlayer(fight.fighter1), std::format("You gain '{}' combat experience.\n\r", xpGainF1));
            }
            if (fight.fighter2->levels.combat != maxLevel) {
                fight.fighter2->xp.combat += xpGainF2;
                send(getConnFromPlayer(fight.fighter2), std::format("You gain '{}' combat experience.\n\r", xpGainF2));
            }

            if (fight.fighter1->xp.combat >= XPToNextLevel(fight.fighter1->levels.combat) && fight.fighter1->levels.combat + 1 < maxLevel) {
                fight.fighter1->xp.combat = 0;
                fight.fighter1->levels.combat++;
                send(getConnFromPlayer(fight.fighter1), std::format("&CYou've gained a level! You're now combat level {}!\n\r", fight.fighter1->levels.combat));
            }

            if (fight.fighter2->xp.combat >= XPToNextLevel(fight.fighter2->levels.combat) && fight.fighter2->levels.combat + 1 < maxLevel) {
                fight.fighter2->xp.combat = 0;
                fight.fighter2->levels.combat++;
                send(getConnFromPlayer(fight.fighter2), std::format("&CYou've gained a level! You're now combat level {}!\n\r", fight.fighter2->levels.combat));
            }
        }
    }

    std::erase_if(fights, [](Fight fight) {return fight.isOver == true; });
}

int main(int argc, char *argv[])
{
    std::chrono::steady_clock::time_point thenTime = std::chrono::high_resolution_clock::now();
    conns.reserve(100); //reserve room for 100 connections so we don't invalidate pointers every time we add one. nasty hack because we don't actually manage memory right now.
    double elapsedTime;
    bool shutdown = false;
    std::string port = DEFAULT_PORT;
    size_t hitGainTickNum = 0;
    size_t fightTickNum = 0;

    if (argc > 1) {
        if (atoi(argv[1]) == 0) {
            printf("Invalid port provided.\n\r");
            printf("Usage: main [port]\n\r");
            return false;
        }
        port = argv[1];
    }

    auto WSE = WinSockEngine();

    MudSocket Listen = MudSocket();
    MudSocket Client = MudSocket();

    char buf[DEFAULT_BUFLEN];
    memset(buf, ' ', sizeof(buf));

    printf("%s starting up on port %d ... \n", MSMUD::mudName.c_str(), atoi(port.c_str()));

    if (!loadAreas()) {
        printf("Failed to load areas... Aborting!\n\r");
        return 1;
    }

    if (!loadHelps()) {
        printf("Failed to load help entries... Aborting!\n\r");
        return 1;
    }

    // Resolve the server address and port
    WSE.iResult = getaddrinfo(NULL, port.c_str(), &Listen.hints, &Listen.result);
    if ( WSE.iResult != 0 ) {
        printf("getaddrinfo failed with error: %d\n", WSE.iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for the server to listen for client connections.
    Listen.sock = socket(Listen.result->ai_family, Listen.result->ai_socktype, Listen.result->ai_protocol);
    if (Listen.sock == INVALID_SOCKET) {
        printf("socket failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(Listen.result);
        WSACleanup();
        return 1;
    }
	

    u_long mode = 1; // 1 to enable non-blocking socket
    ioctlsocket(Listen.sock, FIONBIO, &mode);
    ioctlsocket(Client.sock, FIONBIO, &mode);

	printf("Listening Socket Started.\n");

    // Setup the TCP listening socket
    WSE.iResult = bind( Listen.sock, Listen.result->ai_addr, (int)Listen.result->ai_addrlen);
    if (WSE.iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(Listen.result);
        closesocket(Listen.sock);
        WSACleanup();
        return 1;
    }


    freeaddrinfo(Listen.result);

    WSE.iResult = listen(Listen.sock, SOMAXCONN);
    if (WSE.iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(Listen.sock);
        WSACleanup();
        return 1;
    }
	
    printf("Listening for connections on port: %d.\n", atoi(port.c_str()));
	
	while (!shutdown)
	{

        WSE.iResult = 0;

        sockaddr_in client_info = { 0 };
        int addrsize = sizeof(client_info);
        char* ip = inet_ntoa(client_info.sin_addr);
        
        // Accept a client socket
		Client.sock = accept(Listen.sock, NULL, NULL);

        if (Client.sock != INVALID_SOCKET) {

            getsockname(Client.sock, (struct sockaddr*)&client_info, &addrsize);
            ip = inet_ntoa(client_info.sin_addr);
            if (!std::count_if(conns.begin(), conns.end(), [&Client](const mudConn &currConn) { return currConn.sock.sock == Client.sock; })) {
                mudConn newConn;
                newConn.sock = Client;
                newConn.state = connStates::INIT;
                newConn.config.echo_on = true;
                newConn.config.color_on = true;
                conns.push_back(std::move(newConn));
                printf("Incoming connection from %s...\n", ip);
            }
        }

        for (auto &currConn : conns) {

            getsockname(currConn.sock.sock, (struct sockaddr*)&client_info, &addrsize);
            ip = inet_ntoa(client_info.sin_addr);

            switch (currConn.state) {
            case INIT:
                strcpy_s(buf, WelcomeBanner());
                send(currConn, buf);
                currConn.state = connStates::GETNAME;
                break;

            case GETNAME:
                if (currConn.player.name == "") {
                    strcpy_s(buf, NamePrompt());
                    send(currConn, buf);
                    currConn.player.name = "NEW";
                } 
                else if (currConn.player.name == "NEW")
                {

                }
                else {
                    currConn.state = connStates::GETPASSWORD;
                }
                break;

            case GETPASSWORD:

                break;

            case CONNECTED:
                break;

            case EDITING:
                
                break;
            case CLOSE:
                std::erase_if(getRoom(currConn)->players, [&currConn](auto& player) { return currConn.player.name == player->name; });
                closesocket(currConn.sock.sock);
                currConn.state = connStates::REMOVE;
                break;

            default:
                printf("ERROR: INVALID STATE ON SOCKET.");
                closesocket(currConn.sock.sock);
                break;
            }

            WSE.iResult = 0;
            getsockname(currConn.sock.sock, (struct sockaddr*)&client_info, &addrsize);
            ip = inet_ntoa(client_info.sin_addr);

            WSE.iResult = recv(currConn.sock.sock, currConn.buffer, sizeof(currConn.buffer), 0);

            if (WSE.iResult > 0) {
                bufferToCmdLine(currConn, WSE.iResult);
            }
            else if (WSE.iResult == 0) {
                currConn.state = connStates::CLOSE;
            }

            if (currConn.cmdline.size() > 1) {
                std::string cmd = currConn.cmdline.back();
                currConn.cmdline.pop_back();
                processCmdLine(cmd, currConn);
            }

        }

        conns.erase(std::remove_if(conns.begin(), conns.end(), [](mudConn &conn) {
            return conn.state == connStates::REMOVE;
            }), conns.end());

        elapsedTime = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - thenTime).count();

        if (elapsedTime > GAME_TICK_MILLISECS) {
            thenTime = std::chrono::high_resolution_clock::now();
            hitGainTickNum++;
            fightTickNum++;
            if (hitGainTickNum >= TICKS_PER_HP_GAIN) {
                hitGainTickNum = 0;
                hitGain();
            }
            if (fightTickNum >= TICKS_PER_FIGHT_ROUND) {
                fightTickNum = 0;
                fightRound();
            }
        }

	}
	
	closesocket(Listen.sock);
	closesocket(Client.sock);
    WSACleanup();
    return 0;
}