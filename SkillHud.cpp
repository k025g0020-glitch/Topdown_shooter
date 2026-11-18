#include "SkillHud.h"
#include "Player.h"

using namespace KamataEngine;

SkillHud::~SkillHud() {
	for (uint32_t i = 0; i < kNumIcons; ++i) {
		delete spriteFrames_[i];
		delete spriteIcons_[i];
		delete spriteCooldowns_[i];
	}
}

void SkillHud::Initialize() {
	iconColors_ = {
	    Vector4{1.0f, 0.55f, 0.1f, 1.0f}, // Q: 火炎放射
	    Vector4{0.9f, 0.15f, 0.05f, 1.0f}, // E: 火炎球
	    Vector4{1.0f, 0.85f, 0.2f, 1.0f}, // R: 炎の円
	};

	// 画面下中央に横並び
	float totalWidth = kIconSize * kNumIcons + kIconSpacing * (kNumIcons - 1);
	float startX = (static_cast<float>(WinApp::kWindowWidth) - totalWidth) / 2.0f;
	float y = static_cast<float>(WinApp::kWindowHeight) - kBottomMargin - kIconSize;

	for (uint32_t i = 0; i < kNumIcons; ++i) {
		iconPositions_[i] = {startX + (kIconSize + kIconSpacing) * i, y};

		spriteFrames_[i] = Sprite::Create(0, {iconPositions_[i].x - kBorder, iconPositions_[i].y - kBorder});
		spriteFrames_[i]->SetSize({kIconSize + kBorder * 2.0f, kIconSize + kBorder * 2.0f});
		spriteFrames_[i]->SetColor({0.1f, 0.1f, 0.1f, 0.8f});

		spriteIcons_[i] = Sprite::Create(0, iconPositions_[i]);
		spriteIcons_[i]->SetSize({kIconSize, kIconSize});
		spriteIcons_[i]->SetColor(iconColors_[i]);

		spriteCooldowns_[i] = Sprite::Create(0, iconPositions_[i]);
		spriteCooldowns_[i]->SetSize({kIconSize, 0.0f});
		spriteCooldowns_[i]->SetColor({0.0f, 0.0f, 0.0f, 0.7f});
	}
}

void SkillHud::Update(const Player& player) {
	for (uint32_t i = 0; i < kNumIcons; ++i) {
		Player::SkillId skill = static_cast<Player::SkillId>(i);

		// MPが足りなければ青っぽく暗く、使用中（火炎放射）なら明るく
		Vector4 color = iconColors_[i];
		if (!player.HasEnoughMp(skill)) {
			color = {color.x * 0.3f, color.y * 0.3f, color.z * 0.3f + 0.3f, 1.0f};
		} else if (skill == Player::kSkillFlamethrower && player.IsChanneling()) {
			color = {1.0f, 1.0f, 0.7f, 1.0f};
		}
		spriteIcons_[i]->SetColor(color);

		// クールダウンの残り割合だけ上から覆う
		float ratio = player.GetSkillCooldownRatio(skill);
		spriteCooldowns_[i]->SetSize({kIconSize, kIconSize * ratio});
	}
}

void SkillHud::Draw() {
	Sprite::PreDraw();
	for (uint32_t i = 0; i < kNumIcons; ++i) {
		spriteFrames_[i]->Draw();
		spriteIcons_[i]->Draw();
		spriteCooldowns_[i]->Draw();
	}
	Sprite::PostDraw();
}
