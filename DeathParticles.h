#pragma once
#include "KamataEngine.h"
#include "MapChipField.h"
#include <array>
#include <algorithm>

class MapChipField;

class DeathParticles {
	public:
	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, const KamataEngine::Vector3& position);
	void Update();
	void Draw();

	bool IsFinished() const { return isFinished_; }

private:
	static inline const uint32_t kNumParticles = 8;
	std::array<KamataEngine::WorldTransform, kNumParticles> worldTransform_;

	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;

	static inline const float kDuration = 1.0f;
	static inline const float kSpeed = 0.07f;
	static inline const float kAngleUnit = (2.0f * 3.1415926535f) / kNumParticles;

	bool isFinished_ = false;
	float counter_ = 0.0f;

	// 色変更オブジェクト
	KamataEngine::ObjectColor objectColor_;
	// 色の数値 (RGBA)
	KamataEngine::Vector4 color_;
};
