#define NOMINMAX
#include "FireParticles.h"
#include "MapChipField.h"
#include "calc.h"
#include <algorithm>
#include <random>

using namespace KamataEngine;

namespace {
float RandomRange(float min, float max) {
	static std::mt19937 engine{std::random_device{}()};
	std::uniform_real_distribution<float> dist(min, max);
	return dist(engine);
}

// 経過割合 t (0→1) に応じた炎の色：黄色 → オレンジ → 暗い赤、だんだん透明に
Vector4 FireColor(float t) {
	const Vector3 kYellow = {1.0f, 0.9f, 0.3f};
	const Vector3 kOrange = {1.0f, 0.45f, 0.05f};
	const Vector3 kRed = {0.6f, 0.08f, 0.02f};
	Vector3 rgb = (t < 0.4f) ? calc::Lerp(kYellow, kOrange, t / 0.4f) : calc::Lerp(kOrange, kRed, (t - 0.4f) / 0.6f);
	float alpha = 1.0f - t * t;
	return {rgb.x, rgb.y, rgb.z, alpha};
}
} // namespace

void FireParticles::Initialize(Model* model, Camera* camera, MapChipField* mapChipField) {
	model_ = model;
	camera_ = camera;
	mapChipField_ = mapChipField;

	for (Particle& particle : particles_) {
		particle.worldTransform.Initialize();
		particle.objectColor.Initialize();
		particle.isActive = false;
	}
}

void FireParticles::Emit(const Vector3& position, const Vector3& velocity, int32_t life, float startScale, float endScale) {
	// 空いている粒を探す（見つからなければ一番古い位置の粒を上書き）
	uint32_t index = nextIndex_;
	for (uint32_t i = 0; i < kMaxParticles; ++i) {
		uint32_t candidate = (nextIndex_ + i) % kMaxParticles;
		if (!particles_[candidate].isActive) {
			index = candidate;
			break;
		}
	}
	nextIndex_ = (index + 1) % kMaxParticles;

	Particle& particle = particles_[index];
	particle.isActive = true;
	particle.worldTransform.translation_ = position;
	particle.worldTransform.rotation_ = {RandomRange(0.0f, 3.14f), RandomRange(0.0f, 3.14f), RandomRange(0.0f, 3.14f)};
	particle.velocity = velocity;
	particle.angularSpeed = {RandomRange(-0.2f, 0.2f), RandomRange(-0.2f, 0.2f), RandomRange(-0.2f, 0.2f)};
	particle.life = life;
	particle.maxLife = life;
	particle.startScale = startScale;
	particle.endScale = endScale;
}

void FireParticles::Update() {
	for (Particle& particle : particles_) {
		if (!particle.isActive) {
			continue;
		}

		// 寿命
		if (--particle.life <= 0) {
			particle.isActive = false;
			continue;
		}

		WorldTransform& wt = particle.worldTransform;

		// 移動（だんだん減速し、少しずつ上に昇る）
		wt.translation_ = calc::Add(wt.translation_, particle.velocity);
		particle.velocity = calc::Multiply(kDrag, particle.velocity);
		particle.velocity.y += kRise;

		// 壁の中に入ったら消える
		if (mapChipField_) {
			MapChipField::IndexSet index = mapChipField_->GetMapChipIndexByPosition(wt.translation_);
			if (mapChipField_->GetMapChipTypeByIndex(index.x, index.y) == MapChipType::kBlock) {
				particle.isActive = false;
				continue;
			}
		}

		// 回転
		wt.rotation_ = calc::Add(wt.rotation_, particle.angularSpeed);

		// 大きさと色
		float t = 1.0f - static_cast<float>(particle.life) / static_cast<float>(particle.maxLife);
		float scale = calc::Lerp(particle.startScale, particle.endScale, t);
		wt.scale_ = {scale, scale, scale};
		particle.objectColor.SetColor(FireColor(t));

		wt.matWorld_ = calc::MakeAffineMatrix(wt.scale_, wt.rotation_, wt.translation_);
		wt.TransferMatrix();
	}
}

void FireParticles::Draw() {
	for (Particle& particle : particles_) {
		if (particle.isActive) {
			model_->Draw(particle.worldTransform, *camera_, &particle.objectColor);
		}
	}
}
