#include "Fireball.h"
#include "Enemy.h"
#include "FireParticles.h"
#include "MapChipField.h"
#include "calc.h"
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

void Fireball::Initialize(Model* model, Camera* camera, MapChipField* mapChipField, const std::list<Enemy*>* enemies, FireParticles* fireParticles, const Vector3& position,
                          const Vector3& direction) {
	model_ = model;
	camera_ = camera;
	mapChipField_ = mapChipField;
	enemies_ = enemies;
	fireParticles_ = fireParticles;

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
	worldTransform_.scale_ = {kScale, kScale, kScale};

	objectColor_.Initialize();
	objectColor_.SetColor({1.0f, 0.5f, 0.1f, 1.0f});

	velocity_ = calc::Multiply(kSpeed, direction);
}

void Fireball::Update() {
	if (isDead_) {
		return;
	}

	// 移動
	worldTransform_.translation_ = calc::Add(worldTransform_.translation_, velocity_);
	// 回転させて燃えている感じを出す
	worldTransform_.rotation_ = calc::Add(worldTransform_.rotation_, {0.15f, 0.2f, 0.1f});

	// 炎の尾を引く
	for (int i = 0; i < 3; ++i) {
		Vector3 offset = {RandomRange(-0.4f, 0.4f), RandomRange(-0.4f, 0.4f), RandomRange(-0.4f, 0.4f)};
		fireParticles_->Emit(calc::Add(worldTransform_.translation_, offset), calc::Multiply(-0.1f, velocity_), 20, 0.7f, 0.1f);
	}

	worldTransform_.matWorld_ = calc::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();

	// 壁に当たったら爆発
	MapChipField::IndexSet index = mapChipField_->GetMapChipIndexByPosition(worldTransform_.translation_);
	if (mapChipField_->GetMapChipTypeByIndex(index.x, index.y) == MapChipType::kBlock) {
		Explode();
		return;
	}

	// 敵に当たったら爆発
	const Vector3& pos = worldTransform_.translation_;
	AABB aabb = {{pos.x - kSize / 2.0f, pos.y - kSize / 2.0f, pos.z - kSize / 2.0f}, {pos.x + kSize / 2.0f, pos.y + kSize / 2.0f, pos.z + kSize / 2.0f}};
	for (Enemy* enemy : *enemies_) {
		if (!enemy->IsDead() && calc::IsCollision(aabb, enemy->GetAABB())) {
			Explode();
			return;
		}
	}

	// 射程切れでも爆発
	if (--lifeTimer_ <= 0) {
		Explode();
	}
}

void Fireball::Explode() {
	isDead_ = true;
	const Vector3& center = worldTransform_.translation_;

	// 爆発範囲内（XZ平面の距離）の敵にダメージ。自キャラは対象外
	for (Enemy* enemy : *enemies_) {
		if (enemy->IsDead()) {
			continue;
		}
		Vector3 diff = calc::Subtract(enemy->GetWorldPosition(), center);
		diff.y = 0.0f;
		if (calc::Length(diff) <= kExplosionRadius) {
			enemy->TakeDamage(kExplosionDamage);
		}
	}

	// 爆発の見た目：全方向に炎を飛び散らせる
	const int kNumBurst = 50;
	for (int i = 0; i < kNumBurst; ++i) {
		float angle = RandomRange(0.0f, 2.0f * std::numbers::pi_v<float>);
		float speed = RandomRange(0.1f, 0.25f);
		Vector3 velocity = {std::cos(angle) * speed, RandomRange(0.0f, 0.15f), std::sin(angle) * speed};
		fireParticles_->Emit(center, velocity, 30, 1.2f, 0.2f);
	}
}

void Fireball::Draw() {
	if (!isDead_) {
		model_->Draw(worldTransform_, *camera_, &objectColor_);
	}
}
