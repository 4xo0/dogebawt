#pragma once
#include <cstdint>

struct Keybind {
    int  vk = 0;
    bool listening = false;

    bool Pressed() const;
};

struct Config {
    enum TargetingStyle { TS_DISTANCE = 0, TS_CURSOR = 1, TS_HEALTH = 2 };

    // Menu / presentation.
    int   menuToggleHotkey = 0x2D; // VK_INSERT
    int   menuTheme = 0;
    bool  menuBackground = true;
    bool  titleBarActive = true;
    bool  sideBarBackground = true;
    float colorMenuBackground[4] = { 0.20f, 0.05f, 0.20f, 0.75f };
    float colorTitleActive[4] = { 0.30f, 0.10f, 0.40f, 1.00f };
    float colorSidebar[4] = { 0.25f, 0.05f, 0.30f, 1.00f };
    float colorBase[4] = { 0.80f, 0.20f, 0.60f, 1.00f };
    float colorHover[4] = { 1.00f, 0.40f, 0.80f, 1.00f };
    float colorActive[4] = { 1.00f, 0.20f, 0.70f, 1.00f };
    float colorCheck[4] = { 1.00f, 0.35f, 0.80f, 1.00f };
    float colorText[4] = { 1.00f, 1.00f, 1.00f, 1.00f };

    // Mods.
    bool  useSpeed1 = false;
    bool  showCurrentSpeed = false;
    int   speedHackHotkey = 0;
    float speedhackSpeed1 = 1.0f;
    float speedhackSpeed2 = 2.0f;
    int   speedToggleKey = 0;
    bool  noFog = false;
    bool  socketFu = false;
    bool  showSocketFuTimer = false;
    bool  socketFuUseSecondSpeed = false;
    bool  socketFuRestrictMovement = false;
    bool  socketFuNoClip = false;
    bool  autoNoClip = false;
    bool  antiIdle = false;
    float cameraZoomScale = 1.0f;

    // Auto Nexus.
    bool  autoNexus = true;
    bool  autoNexusDisplay = true;
    float autoNexusHpValue = 350.0f;
    bool  autoNexusUsePercent = false;
    float autoNexusHpPercent = 25.0f;
    bool  detectServerHits = true;

    // Auto Aim.
    bool  autoAim = true;
    bool  magnetAim = true;
    bool  magnetRangeExt = true;
    int   targetingStyle = TS_CURSOR;
    float magnetAimRange = 1.8f;
    bool  projectileNoClip = true;
    bool  renderAimInfo = true;
    Keybind aimbotHotkey;

    // Auto Dodge.
    bool  autoDodge = false;
    bool  dodgeProjectiles = true;
    bool  dodgeHoldToToggle = true;
    Keybind dodgingHotkey;
    bool  dodgeInvisible = false;
    bool  butterWalk = true;
    float dodgeHitboxSize = 0.451f;
    int   dodgeMoveAwayMs = 200;
    bool  dodgeAoeBombs = true;
    bool  dodgeAvoidUnits = true;
    float dodgeUnitAvoidanceScale = 1.0f;
    bool  oldDodgeLogic = false;
    bool  teleportIfOutOfRange = false;
    bool  nexusWhenLost = false;
    float dogeTeleportMax = 0.0f;

    // Render / loot.
    bool  enablePoisBags = true;
    bool  playSoundForBags = true;
    bool  bagEgg = true;
    bool  bagBrown = true;
    bool  bagPink = true;
    bool  bagPurple = true;
    bool  bagCyan = true;
    bool  bagDarkBlue = true;
    bool  bagWhite = true;
    bool  bagGold = true;
    bool  bagOrange = true;
    bool  bagRed = true;
    bool  renderProjectiles = false;
    bool  renderAoeDebug = false;
    bool  renderTiles = false;
    bool  renderUnits = false;
    bool  renderHitbox = false;
    bool  renderGrid = false;
    bool  renderSafetyPath = false;

    // Misc / stats.
    bool  enableGlow = false;
    bool  rainbowGlow = false;
    int   glowStyle = 0;
    bool  showFpm = false;
    bool  fameValue = false;
    float fameValueAmount = 0.0f;
    bool  accountFame = false;
    float accountFameValue = 0.0f;
};

extern Config g_cfg;

void Config_Load();
void Config_Save();
const char* Config_Path();
