#pragma once

#define maxLevel 105

struct stats {
    int strength;
    int intellect;
    int dexterity;
    int constitution;
};

struct levels {
    int combat;
    int piloting;
    int engineering;
    int leadership;
};

struct experience {
    size_t combat;
    size_t piloting;
    size_t engineering;
    size_t leadership;
};

size_t XPToNextLevel(size_t currLevel) {
    return (currLevel * currLevel * 500);
}

enum Ranks {
    PRIVATE = 1,
    PRIVATESECONDCLASS = 2,
    PRIVATEFIRSTCLASS = 3,
    SPECIALIST = 4,
    CORPORAL = 5,
    SERGEANT = 6,
    STAFFSERGEANT = 7,
    SERGEANTFIRSTCLASS = 8,
    MASTERSERGEANT = 9,
    FIRSTSERGEANT = 10,
    SERGEANTMAJOR = 11,
    WARRANTOFFICER = 12,
    SECONDLIEUTENANT = 13,
    FIRSTLIEUTENANT = 14,
    CAPTAIN = 15,
    MAJOR = 16,
    LIEUTENANTCOLONEL = 17,
    COLONEL = 18,
    BRIGADIERGENERAL = 19,
    MAJORGENERAL = 20,
    LIEUTENANTGENERAL = 21,
    GENERAL = 22
};

enum playerState {
    SLEEPING = 1,
    RESTING = 2,
    SITTING = 3,
    STANDING = 4,
    FIGHTING = 5,
};

class Player {
public:
    std::string name;
    std::string password;
    std::string title;
    size_t state;
    int hp;
    int maxhp;
    int focus;
    stats stats;
    levels levels;
    experience xp;
    size_t roomVnum;
    time_t loggedInTime;
    time_t lastTimeSaved;

    static const size_t BASE_STAT = 5;

    Player() {
        name = "";
        password = "";
        title = "";
        state = STANDING;
        hp = 100;
        maxhp = 100;
        focus = 1;
        stats.strength = BASE_STAT;
        stats.intellect = BASE_STAT;
        stats.dexterity = BASE_STAT;
        stats.constitution = BASE_STAT;
        levels.combat = 1;
        levels.piloting = 1;
        levels.engineering = 1;
        levels.leadership = 1;
        xp.combat = 0;
        xp.piloting = 0;
        xp.engineering = 0;
        xp.leadership = 0;
        roomVnum = 50;
        loggedInTime = time(nullptr);
    }
};