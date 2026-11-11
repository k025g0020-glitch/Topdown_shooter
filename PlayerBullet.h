#pragma once
#include "KamataEngine.h"
#include "CollisionMapInfo.h"

class MapChipField;

// 自キャラの弾
class PlayerBullet {
public:
	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, MapChipField* mapChipField, const KamataEngine::Vector3& position, const KamataEngine::Vector3& velocity);
	void Update();
	void Draw();

	bool IsDead() const { return isDead_; }

	KamataEngine::Vector3 GetWorldPosition() const;
	AABB GetAABB() const;

	// 敵に当たったときの処理
	void OnCollision();

private:
	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
	MapChipField* mapChipField_ = nullptr;

	// 速度（XZ平面）
	KamataEngine::Vector3 velocity_ = {};

	// 寿命<フレーム>
	static inline const int32_t kLifeTime = 60 * 2;
	int32_t deathTimer_ = kLifeTime;

	// 当たり判定サイズ
	static inline const float kSize = 0.4f;

	bool isDead_ = false;
};
