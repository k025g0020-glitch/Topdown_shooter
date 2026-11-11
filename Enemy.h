#pragma once
#include "KamataEngine.h"
#include "CollisionMapInfo.h"
#include <cassert>

class MapChipField;

class Player;

class Enemy {
public:
	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, MapChipField* mapChipField, const KamataEngine::Vector3& position);
	void Update();
	void Draw();

	const KamataEngine::Vector3& GetTranslation() const { return worldTransform_.translation_; }
	void SetTranslation(const KamataEngine::Vector3& translation) { worldTransform_.translation_ = translation; }
	const KamataEngine::WorldTransform& GetWorldTransform() const { return worldTransform_; }
	const KamataEngine::Vector3& GetVelocity() const { return velocity_; }

	void SetMapChipField(MapChipField* mapChipField) { mapChipField_ = mapChipField; }

	KamataEngine::Vector3 GetWorldPosition();
	AABB GetAABB();

	void OnCollision(const Player* player);
	// 弾に当たったときの処理
	void OnBulletCollision();

	bool IsDead() const { return isDead_; }

private:
	// 上から見たときの4つの角（Far = 奥(+Z), Near = 手前(-Z)）
	enum Corner {
		kRightNear, // 右手前
		kLeftNear,  // 左手前
		kRightFar,  // 右奥
		kLeftFar,   // 左奥
		kNumCorner  // 要素数
	};

	void Move();

	// 判定結果を反映して移動
	void ApplyMove(const CollisionMapInfo& info);

	void OnWallCollision(const CollisionMapInfo& info);

	// 角の座標計算関数
	KamataEngine::Vector3 CornerPosition(const KamataEngine::Vector3& center, Corner corner);

	// マップ衝突判定の主関数
	void MapCollision(CollisionMapInfo& info);

	// 各方向の衝突判定小分け関数
	void MapCollisionFar(CollisionMapInfo& info);   // 奥 (+Z)
	void MapCollisionNear(CollisionMapInfo& info);  // 手前 (-Z)
	void MapCollisionRight(CollisionMapInfo& info); // 右 (+X)
	void MapCollisionLeft(CollisionMapInfo& info);  // 左 (-X)

	enum class LRDirection {
		kRight,
		kLeft,
	};

	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;

	KamataEngine::Vector3 velocity_ = {};
	MapChipField* mapChipField_ = nullptr;

	LRDirection lrDirection_ = LRDirection::kRight;

	// 旋回制御用メンバ
	float turnFirstRotationY_ = 0.0f;
	float turnTimer_ = 0.0f;
	static inline const float kTimeTurn = 0.3f;

	// エネミー用の移動パラメータ
	static inline const float kAcceleration = 0.05f;
	static inline const float kLimitRunSpeed = 0.1f; // プレイヤーより遅め

	// 当たり判定サイズ
	static inline const float kWidth = 0.8f;
	static inline const float kHeight = 0.8f;
	static inline const float kBlank = 0.005f;

	// 壁接触時の速度減衰率
	static inline const float kAttenuationWall = 0.2f;

	// アニメーション
	static inline const float kWalkMotionAngleStart = 30.0f;
	static inline const float kWalkMotionAngleEnd = 30.0f;
	float walkTimer_ = 0.0f;

	bool isDead_ = false;
};
