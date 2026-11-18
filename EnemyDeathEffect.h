#pragma once
#include "KamataEngine.h"
#include <array>

// 敵を倒したときに飛び散るエフェクト
class EnemyDeathEffect {
public:
	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, const KamataEngine::Vector3& position);
	void Update();
	void Draw();

	bool IsFinished() const { return isFinished_; }

private:
	// 破片の数
	static inline const uint32_t kNumParticles = 12;

	// 破片1つ分の情報
	struct Particle {
		KamataEngine::WorldTransform worldTransform;
		KamataEngine::Vector3 velocity;     // 速度
		KamataEngine::Vector3 angularSpeed; // 回転速度
	};
	std::array<Particle, kNumParticles> particles_;

	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;

	// 持続時間<秒>
	static inline const float kDuration = 0.8f;
	// 水平方向の速さ（最小・最大）
	static inline const float kSpeedMin = 0.08f;
	static inline const float kSpeedMax = 0.18f;
	// 打ち上げの初速と重力
	static inline const float kJumpSpeed = 0.15f;
	static inline const float kGravity = 0.015f;
	// 床の高さ（これより下には落ちない）
	static inline const float kFloorY = -0.3f;
	// 破片の初期スケール
	static inline const float kStartScale = 0.8f;

	float counter_ = 0.0f;
	bool isFinished_ = false;

	// 色変更オブジェクト
	KamataEngine::ObjectColor objectColor_;
	KamataEngine::Vector4 color_;
};
