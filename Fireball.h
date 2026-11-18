#pragma once
#include "KamataEngine.h"
#include "CollisionMapInfo.h"
#include <list>

class MapChipField;
class Enemy;
class FireParticles;

// 火炎球（スキル）
// 飛んでいき、敵か壁に当たる（または射程切れ）と爆発して周りの敵にダメージ
class Fireball {
public:
	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, MapChipField* mapChipField, const std::list<Enemy*>* enemies, FireParticles* fireParticles,
	                const KamataEngine::Vector3& position, const KamataEngine::Vector3& direction);
	void Update();
	void Draw();

	bool IsDead() const { return isDead_; }

private:
	// 爆発させる
	void Explode();

	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::ObjectColor objectColor_;
	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
	MapChipField* mapChipField_ = nullptr;
	const std::list<Enemy*>* enemies_ = nullptr;
	FireParticles* fireParticles_ = nullptr;

	KamataEngine::Vector3 velocity_ = {};

	static inline const float kSpeed = 0.3f;
	static inline const float kScale = 1.5f;           // 見た目の大きさ
	static inline const float kSize = 1.0f;            // 当たり判定の大きさ
	static inline const int32_t kLifeTime = 90;        // 射程切れまで<フレーム>
	static inline const float kExplosionRadius = 2.5f; // 爆発の半径
	static inline const int32_t kExplosionDamage = 40; // 爆発のダメージ

	int32_t lifeTimer_ = kLifeTime;
	bool isDead_ = false;
};
