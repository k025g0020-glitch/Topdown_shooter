#define NOMINMAX
#include "HpGauge.h"
#include <algorithm>

using namespace KamataEngine;

HpGauge::~HpGauge() {
	delete spriteBack_;
	delete spriteDamage_;
	delete spriteFill_;
}

void HpGauge::Initialize(const Vector2& position, const Vector2& size, const Vector4& color) {
	size_ = size;

	// テクスチャハンドル0（白）を色付けして使う（Fade と同じ方法）
	spriteBack_ = Sprite::Create(0, {position.x - kBorder, position.y - kBorder});
	spriteBack_->SetSize({size.x + kBorder * 2.0f, size.y + kBorder * 2.0f});
	spriteBack_->SetColor({0.1f, 0.1f, 0.1f, 0.8f});

	spriteDamage_ = Sprite::Create(0, position);
	spriteDamage_->SetSize(size);
	spriteDamage_->SetColor({1.0f, 1.0f, 1.0f, 0.8f});

	spriteFill_ = Sprite::Create(0, position);
	spriteFill_->SetSize(size);
	spriteFill_->SetColor(color);

	damageRatio_ = 1.0f;
}

void HpGauge::Update(float value, float maxValue) {
	float ratio = (maxValue > 0.0f) ? std::clamp(value / maxValue, 0.0f, 1.0f) : 0.0f;

	// 白いゲージは少しずつ現在HPに追いつく（回復したときはすぐ合わせる）
	if (damageRatio_ > ratio) {
		damageRatio_ = std::max(ratio, damageRatio_ - kDamageFollowRate * (damageRatio_ - ratio) - 0.002f);
	} else {
		damageRatio_ = ratio;
	}

	spriteFill_->SetSize({size_.x * ratio, size_.y});
	spriteDamage_->SetSize({size_.x * damageRatio_, size_.y});
}

void HpGauge::Draw() {
	Sprite::PreDraw();
	spriteBack_->Draw();
	spriteDamage_->Draw();
	spriteFill_->Draw();
	Sprite::PostDraw();
}
