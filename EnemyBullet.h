#pragma once
#include "KamataEngine.h"
#include "CollisionMapInfo.h"

class MapChipField;

// 敵（遠距離型）の弾
class EnemyBullet {
public:
	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, MapChipField* mapChipField, const KamataEngine::Vector3& position, const KamataEngine::Vector3& velocity);
	void Update();
	void Draw();

	bool IsDead() const { return isDead_; }
	int32_t GetDamage() const { return kDamage; }

	AABB GetAABB() const;

	// 自キャラに当たったときの処理
	void OnCollision() { isDead_ = true; }

private:
	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::ObjectColor objectColor_;
	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
	MapChipField* mapChipField_ = nullptr;

	KamataEngine::Vector3 velocity_ = {};

	static inline const int32_t kDamage = 15;
	static inline const int32_t kLifeTime = 60 * 4; // 寿命<フレーム>
	static inline const float kScale = 0.7f;        // 見た目の大きさ
	static inline const float kSize = 0.35f;        // 当たり判定の大きさ（見た目より小さめで避けやすく）

	int32_t deathTimer_ = kLifeTime;
	int32_t frameCount_ = 0;
	bool isDead_ = false;
};
