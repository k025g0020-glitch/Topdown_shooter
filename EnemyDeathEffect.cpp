#define NOMINMAX
#include "EnemyDeathEffect.h"
#include "calc.h"
#include <algorithm>
#include <numbers>
#include <random>

using namespace KamataEngine;

namespace {
// 乱数生成
std::mt19937& RandomEngine() {
	static std::mt19937 engine{std::random_device{}()};
	return engine;
}
float RandomRange(float min, float max) {
	std::uniform_real_distribution<float> dist(min, max);
	return dist(RandomEngine());
}
} // namespace

void EnemyDeathEffect::Initialize(Model* model, Camera* camera, const Vector3& position) {
	model_ = model;
	camera_ = camera;

	const float kPi = std::numbers::pi_v<float>;
	const float kAngleUnit = 2.0f * kPi / static_cast<float>(kNumParticles);

	for (uint32_t i = 0; i < kNumParticles; ++i) {
		Particle& particle = particles_[i];
		particle.worldTransform.Initialize();
		particle.worldTransform.translation_ = position;
		particle.worldTransform.scale_ = {kStartScale, kStartScale, kStartScale};

		// XZ平面上で放射状に（少しばらつかせる）
		float angle = kAngleUnit * static_cast<float>(i) + RandomRange(-0.2f, 0.2f);
		float speed = RandomRange(kSpeedMin, kSpeedMax);
		particle.velocity = {std::cos(angle) * speed, kJumpSpeed * RandomRange(0.5f, 1.2f), std::sin(angle) * speed};

		// くるくる回転させる
		particle.angularSpeed = {RandomRange(-0.3f, 0.3f), RandomRange(-0.3f, 0.3f), RandomRange(-0.3f, 0.3f)};
	}

	// 敵らしいオレンジ色
	objectColor_.Initialize();
	color_ = {1.0f, 0.5f, 0.1f, 1.0f};
	objectColor_.SetColor(color_);

	counter_ = 0.0f;
	isFinished_ = false;
}

void EnemyDeathEffect::Update() {
	if (isFinished_) {
		return;
	}

	// 時間を進める
	counter_ += 1.0f / 60.0f;
	if (counter_ >= kDuration) {
		counter_ = kDuration;
		isFinished_ = true;
	}
	// 経過割合 0.0 → 1.0
	float t = counter_ / kDuration;

	for (Particle& particle : particles_) {
		WorldTransform& wt = particle.worldTransform;

		// 重力
		particle.velocity.y -= kGravity;

		// 移動
		wt.translation_ = calc::Add(wt.translation_, particle.velocity);

		// 床に着いたら止まる（水平方向は摩擦で減速）
		if (wt.translation_.y < kFloorY) {
			wt.translation_.y = kFloorY;
			particle.velocity.y = 0.0f;
			particle.velocity.x *= 0.8f;
			particle.velocity.z *= 0.8f;
		}

		// 回転
		wt.rotation_ = calc::Add(wt.rotation_, particle.angularSpeed);

		// だんだん小さくなる
		float scale = kStartScale * (1.0f - t);
		wt.scale_ = {scale, scale, scale};

		wt.matWorld_ = calc::MakeAffineMatrix(wt.scale_, wt.rotation_, wt.translation_);
		wt.TransferMatrix();
	}

	// フェードアウト
	color_.w = std::clamp(1.0f - t, 0.0f, 1.0f);
	objectColor_.SetColor(color_);
}

void EnemyDeathEffect::Draw() {
	if (isFinished_) {
		return;
	}
	for (Particle& particle : particles_) {
		model_->Draw(particle.worldTransform, *camera_, &objectColor_);
	}
}
