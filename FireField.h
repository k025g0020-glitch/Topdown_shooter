#pragma once
#include "KamataEngine.h"
#include <list>

class Enemy;
class FireParticles;

// 地面に円状に広がる炎（スキル）
// 発動した位置に一定時間残り、範囲内の敵に定期的にダメージ
class FireField {
public:
	void Initialize(const std::list<Enemy*>* enemies, FireParticles* fireParticles, const KamataEngine::Vector3& center);
	void Update();

	bool IsDead() const { return isDead_; }

private:
	const std::list<Enemy*>* enemies_ = nullptr;
	FireParticles* fireParticles_ = nullptr;
	KamataEngine::Vector3 center_ = {};

	static inline const float kRadius = 3.5f;          // 炎の円の半径
	static inline const int32_t kDuration = 180;       // 持続時間<フレーム>
	static inline const int32_t kSpreadTime = 15;      // 円が広がりきるまで<フレーム>
	static inline const int32_t kTickInterval = 15;    // ダメージ間隔<フレーム>
	static inline const int32_t kTickDamage = 8;       // 1回のダメージ
	static inline const float kGroundY = -0.3f;        // 炎を出す高さ（床の少し上）

	int32_t timer_ = 0;
	int32_t tickTimer_ = 0;
	bool isDead_ = false;
};
