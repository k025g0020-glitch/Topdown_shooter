#pragma once
#include "KamataEngine.h"
#include <array>

class Player;

// スキルアイコン（画面下中央）。クールダウン中は暗く覆い、MP不足なら青っぽく暗くする
class SkillHud {
public:
	~SkillHud();
	void Initialize();
	void Update(const Player& player);
	void Draw();

private:
	static inline const uint32_t kNumIcons = 3;
	static inline const float kIconSize = 64.0f;
	static inline const float kIconSpacing = 16.0f;
	static inline const float kBorder = 3.0f;
	static inline const float kBottomMargin = 40.0f;

	std::array<KamataEngine::Sprite*, kNumIcons> spriteFrames_ = {};    // 枠
	std::array<KamataEngine::Sprite*, kNumIcons> spriteIcons_ = {};     // アイコン（色）
	std::array<KamataEngine::Sprite*, kNumIcons> spriteCooldowns_ = {}; // クールダウンの覆い

	std::array<KamataEngine::Vector2, kNumIcons> iconPositions_ = {};
	// アイコンの色（Q:火炎放射, E:火炎球, R:炎の円）
	std::array<KamataEngine::Vector4, kNumIcons> iconColors_ = {};
};
