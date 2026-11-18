#pragma once
#include "KamataEngine.h"

// HP・MPバー（画面に表示するゲージ）
class HpGauge {
public:
	~HpGauge();

	// position: 左上の座標, size: ゲージの大きさ（ピクセル）, color: ゲージの色
	void Initialize(const KamataEngine::Vector2& position, const KamataEngine::Vector2& size, const KamataEngine::Vector4& color = {0.8f, 0.1f, 0.1f, 1.0f});
	// 現在値と最大値からゲージの長さを更新（HP・MP共通）
	void Update(float value, float maxValue);
	void Draw();

private:
	KamataEngine::Sprite* spriteBack_ = nullptr;   // 背景（枠）
	KamataEngine::Sprite* spriteDamage_ = nullptr; // 減った分を遅れて縮める白いゲージ
	KamataEngine::Sprite* spriteFill_ = nullptr;   // 現在HPのゲージ

	KamataEngine::Vector2 size_ = {};

	// 白いゲージの割合（現在HPの割合に遅れて追いつく）
	float damageRatio_ = 1.0f;
	static inline const float kDamageFollowRate = 0.05f;

	// 枠の太さ
	static inline const float kBorder = 3.0f;
};
