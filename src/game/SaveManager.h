#pragma once
#include <string>

namespace vc {

class SaveManager {
public:
    explicit SaveManager(const std::string& baseDir) : baseDir_(baseDir) {}

    std::string worldDir() const { return baseDir_ + "/world"; }
    std::string playerFile() const { return baseDir_ + "/player.bin"; }

    void ensureDirs();
    bool savePlayer(const struct PlayerSaveData& data);
    bool loadPlayer(PlayerSaveData& data);

private:
    std::string baseDir_;
};

struct PlayerSaveData {
    float x, y, z;
    float yaw, pitch;
    float timeOfDay;
};

} // namespace vc
