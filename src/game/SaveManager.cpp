#include "SaveManager.h"
#include <sys/stat.h>
#include <cstdio>
#include <cstring>

namespace vc {

void SaveManager::ensureDirs() {
    mkdir(baseDir_.c_str(), 0700);
    mkdir(worldDir().c_str(), 0700);
}

bool SaveManager::savePlayer(const PlayerSaveData& d) {
    FILE* f = fopen(playerFile().c_str(), "wb");
    if (!f) return false;
    const char magic[4] = {'V','C','P','1'};
    fwrite(magic, 1, 4, f);
    fwrite(&d, sizeof(PlayerSaveData), 1, f);
    fclose(f);
    return true;
}

bool SaveManager::loadPlayer(PlayerSaveData& d) {
    FILE* f = fopen(playerFile().c_str(), "rb");
    if (!f) return false;
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "VCP1", 4) != 0) {
        fclose(f); return false;
    }
    if (fread(&d, sizeof(PlayerSaveData), 1, f) != 1) {
        fclose(f); return false;
    }
    fclose(f);
    return true;
}

} // namespace vc
