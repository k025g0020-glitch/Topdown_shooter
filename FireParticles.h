#pragma once
#include "KamataEngine.h"
#include <array>

class MapChipField;

// 炎の見た目用パーティクル
// 毎フレーム new/delete しないよう、あらかじめ確保した粒を使い回す（プール方式）
class FireParticles {
public:
	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, MapChipField* mapChipField);

	// 粒を1つ出す
	// position: 出す位置, velocity: 初速, life: 寿命<フレーム>, startScale/endScale: 出現時と消える時の大きさ
	void Emit(const KamataEngine::Vector3& position, const KamataEngine::Vector3& velocity, int32_t life, float startScale, float endScale);

	void Update();
	void Draw();

private:
	struct Particle {
		KamataEngine::WorldTransform worldTransform;
		KamataEngine::ObjectColor objectColor;
		KamataEngine::Vector3 velocity;
		KamataEngine::Vector3 angularSpeed;
		int32_t life = 0;
		int32_t maxLife = 0;
		float startScale = 1.0f;
		float endScale = 1.0f;
		bool isActive = false;
	};

	static inline const uint32_t kMaxParticles = 512;
	std::array<Particle, kMaxParticles> particles_;
	// 次に探し始める位置
	uint32_t nextIndex_ = 0;

	// 速度の減衰率と、熱で上に昇る加速度
	static inline const float kDrag = 0.95f;
	static inline const float kRise = 0.004f;

	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
	MapChipField* mapChipField_ = nullptr;
};
