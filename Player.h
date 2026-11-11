#pragma once
#include "CollisionMapInfo.h"
#include "KamataEngine.h"
#include "PlayerBullet.h"
#include <cassert>
#include <list>

class MapChipField;

class Enemy;

class Player {
public:
	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, MapChipField* mapChipField, const KamataEngine::Vector3& position);
	void Update();
	void Draw();

	// 弾のモデルを設定
	void SetBulletModel(KamataEngine::Model* model) { modelBullet_ = model; }
	// 弾リストを取得
	const std::list<PlayerBullet*>& GetBullets() const { return bullets_; }

	~Player();

	const KamataEngine::Vector3& GetTranslation() const { return worldTransform_.translation_; }
	void SetTranslation(const KamataEngine::Vector3& translation) { worldTransform_.translation_ = translation; }
	const KamataEngine::WorldTransform& GetWorldTransform() const { return worldTransform_; }
	const KamataEngine::Vector3& GetVelocity() const { return velocity_; }

	void SetMapChipField(MapChipField* mapChipField) { mapChipField_ = mapChipField; }

	KamataEngine::Vector3 GetWorldPosition();
	AABB GetAABB();

	void OnCollision(const Enemy* enemy);

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

	// 攻撃（弾の発射）
	void Attack();

	// マウスカーソルの方向を狙う
	void Aim();

	// マウスカーソルが指している地面（自キャラと同じ高さの平面）上の座標を求める
	bool GetMouseWorldPosition(KamataEngine::Vector3& outPosition) const;

	// 進行方向へ向きを補間する
	void Turn();

	// 判定結果を反映して移動
	void ApplyMove(const CollisionMapInfo& info);

	// 壁に接触している場合の処理
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

	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;

	KamataEngine::Vector3 velocity_ = {};

	MapChipField* mapChipField_ = nullptr;

	// 向きたい角度（Y軸回転）
	float targetRotationY_ = 0.0f;
	// 旋回の補間率（1フレームあたり）
	static inline const float kTurnRate = 0.3f;

	static inline const float kAcceleration = 0.05f;
	static inline const float kAttenuation = 0.2f;
	static inline const float kLimitRunSpeed = 0.2f;

	// キャラクターの当たり判定サイズ（X幅・Z奥行き・Y高さ）
	static inline const float kWidth = 0.8f;
	static inline const float kHeight = 0.8f;

	static inline const float kBlank = 0.005f;

	// 壁接触時の速度減衰率
	static inline const float kAttenuationWall = 0.2f;

	bool isDead_ = false;

	// 弾
	KamataEngine::Model* modelBullet_ = nullptr;
	std::list<PlayerBullet*> bullets_;
	// 狙っている方向（XZ平面の単位ベクトル）
	KamataEngine::Vector3 aimDirection_ = {0.0f, 0.0f, 1.0f};
	// 弾の速さ
	static inline const float kBulletSpeed = 0.5f;
	// 発射間隔<フレーム>
	static inline const int32_t kFireInterval = 10;
	int32_t fireTimer_ = 0;
};
