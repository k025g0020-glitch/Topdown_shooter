#define NOMINMAX
#include "FireField.h"
#include "Enemy.h"
#include "FireParticles.h"
#include "calc.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

using namespace KamataEngine;

namespace {
float RandomRange(float min, float max) {
	static std::mt19937 engine{std::random_device{}()};
	std::uniform_real_distribution<float> dist(min, max);
	return dist(engine);
}
} // namespace

void FireField::Initialize(const std::list<Enemy*>* enemies, FireParticles* fireParticles, const Vector3& center) {
	enemies_ = enemies;
	fireParticles_ = fireParticles;
	center_ = center;
	timer_ = 0;
	tickTimer_ = 0;
	isDead_ = false;
}

void FireField::Update() {
	if (isDead_) {
		return;
	}

	++timer_;
	if (timer_ >= kDuration) {
		isDead_ = true;
		return;
	}

	// 発動直後は中心から外へ広がる
	float radius = kRadius * std::min(1.0f, static_cast<float>(timer_) / static_cast<float>(kSpreadTime));
	const float kTwoPi = 2.0f * std::numbers::pi_v<float>;

	// 見た目：円の縁に炎を並べる
	for (int i = 0; i < 6; ++i) {
		float angle = RandomRange(0.0f, kTwoPi);
		Vector3 pos = {center_.x + std::cos(angle) * radius, kGroundY, center_.z + std::sin(angle) * radius};
		fireParticles_->Emit(pos, {0.0f, RandomRange(0.02f, 0.06f), 0.0f}, 20, 0.6f, 0.1f);
	}
	// 見た目：円の内側にもまばらに炎を出す（sqrtで中心に偏らないようにする）
	for (int i = 0; i < 4; ++i) {
		float angle = RandomRange(0.0f, kTwoPi);
		float r = radius * std::sqrt(RandomRange(0.0f, 1.0f));
		Vector3 pos = {center_.x + std::cos(angle) * r, kGroundY, center_.z + std::sin(angle) * r};
		fireParticles_->Emit(pos, {0.0f, RandomRange(0.01f, 0.04f), 0.0f}, 18, 0.4f, 0.05f);
	}

	// 一定間隔で範囲内の敵にダメージ（自キャラは対象外）
	if (--tickTimer_ <= 0) {
		tickTimer_ = kTickInterval;
		for (Enemy* enemy : *enemies_) {
			if (enemy->IsDead()) {
				continue;
			}
			Vector3 diff = calc::Subtract(enemy->GetWorldPosition(), center_);
			diff.y = 0.0f;
			if (calc::Length(diff) <= radius) {
				enemy->TakeDamage(kTickDamage);
			}
		}
	}
}
