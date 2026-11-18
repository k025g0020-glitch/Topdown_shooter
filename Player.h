#pragma once
#include "KamataEngine.h"
#include "CollisionMapInfo.h"
#include "PlayerBullet.h"
#include <array>
#include <cassert>
#include <list>

class MapChipField;
class Enemy;
class Fireball;
class FireField;
class FireParticles;

class Player {
public:
	// スキルの種類
	enum SkillId {
		kSkillFlamethrower, // Q: 火炎放射（押している間）
		kSkillFireball,     // E: 火炎球
		kSkillFireRing,     // R: 炎の円
		kNumSkills
	};

	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, MapChipField* mapChipField, const KamataEngine::Vector3& position);
	void Update();
	void Draw();

	// 弾のモデルを設定
	void SetBulletModel(KamataEngine::Model* model) { modelBullet_ = model; }
	// スキルの初期化（火炎球・炎のパーティクルに使うモデルを渡す）
	void InitializeSkills(KamataEngine::Model* fireModel);
	// 狙える敵のリストを設定（GameScene が持つリストを参照する）
	void SetEnemies(const std::list<Enemy*>* enemies) { enemies_ = enemies; }
	// 狙っている敵（いなければ nullptr）
	const Enemy* GetTargetEnemy() const { return targetEnemy_; }
	// カーソルが乗っている敵（いなければ nullptr）
	const Enemy* GetHoveredEnemy() const { return hoveredEnemy_; }

	// 弾リストを取得
	const std::list<PlayerBullet*>& GetBullets() const { return bullets_; }

	~Player();

	const KamataEngine::Vector3& GetTranslation() const { return worldTransform_.translation_; }
	void SetTranslation(const KamataEngine::Vector3& translation) { worldTransform_.translation_ = translation; }
	const KamataEngine::WorldTransform& GetWorldTransform() const { return worldTransform_; }
	const KamataEngine::Vector3& GetVelocity() const { return velocity_; }
	// 前のフレームで実際に動いた量（壁に当たって止まっていれば 0）
	const KamataEngine::Vector3& GetMoveAmount() const { return moveAmount_; }

	void SetMapChipField(MapChipField* mapChipField) { mapChipField_ = mapChipField; }

	KamataEngine::Vector3 GetWorldPosition();
	AABB GetAABB();

	void OnCollision(const Enemy* enemy);

	bool IsDead() const { return isDead_; }

	// ダメージを受ける
	void TakeDamage(int32_t damage);

	int32_t GetHp() const { return hp_; }
	int32_t GetMaxHp() const { return kMaxHp; }
	bool IsInvincible() const { return invincibleTimer_ > 0; }
	bool IsRolling() const { return isRolling_; }

	// MP
	float GetMp() const { return mp_; }
	float GetMaxMp() const { return kMaxMp; }

	// スキルの状態（UI表示用）
	// クールダウンの残り割合（1.0 = 使った直後, 0.0 = 使える）
	float GetSkillCooldownRatio(SkillId skill) const;
	// スキルを使うのに十分なMPがあるか
	bool HasEnoughMp(SkillId skill) const;
	// 火炎放射中か
	bool IsChanneling() const { return isChanneling_; }

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

	// スキルの更新（入力・発動・MP回復）
	void UpdateSkills();
	// 火炎放射（押している間、毎フレーム呼ばれる）
	void UpdateFlamethrower();

	// 前転の開始判定
	void StartRoll();
	// 前転中の更新（回転演出と終了判定）
	void UpdateRoll();

	// マウスカーソルの方向を狙う
	void Aim();

	// マウスカーソルを通るレイ（始点と方向）を求める
	void GetMouseRay(KamataEngine::Vector3& outOrigin, KamataEngine::Vector3& outDirection) const;

	// マウスカーソルが指している地面（自キャラと同じ高さの平面）上の座標を求める
	bool GetMouseWorldPosition(KamataEngine::Vector3& outPosition) const;

	// マウスカーソルの下にいる敵を探す（いなければ nullptr）
	Enemy* FindEnemyUnderCursor() const;

	// 狙う敵の更新（敵の上で左クリックしたら、その敵を狙い続ける）
	void UpdateTarget();

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
	// 実際に動いた量（壁との当たり判定の後の移動量）
	KamataEngine::Vector3 moveAmount_ = {};

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

	// HP
	static inline const int32_t kMaxHp = 100;
	int32_t hp_ = kMaxHp;

	// 被弾後の無敵時間<フレーム>（この間は点滅し、ダメージを受けない）
	static inline const int32_t kInvincibleTime = 60;
	int32_t invincibleTimer_ = 0;

	// MP
	static inline const float kMaxMp = 100.0f;
	static inline const float kMpRegen = 0.15f; // 1フレームあたりの回復量（火炎放射中は回復しない）
	float mp_ = kMaxMp;

	// スキルごとの設定
	// 火炎放射: 押している間MPを消費、クールダウンなし
	// 火炎球・炎の円: 使うときにMPを消費し、クールダウンが始まる
	static inline const std::array<float, kNumSkills> kSkillMpCost = {0.5f, 20.0f, 35.0f}; // 火炎放射は1フレームあたり
	static inline const std::array<int32_t, kNumSkills> kSkillCooldown = {0, 45, 360};      // <フレーム>
	std::array<int32_t, kNumSkills> skillCooldownTimers_ = {};

	// 火炎放射
	bool isChanneling_ = false;
	int32_t flameTickTimer_ = 0;
	static inline const float kFlameRange = 4.0f;           // 届く距離
	static inline const float kFlameHalfAngleDeg = 25.0f;   // 扇形の半分の角度<度>
	static inline const int32_t kFlameTickInterval = 6;     // ダメージ間隔<フレーム>
	static inline const int32_t kFlameDamage = 3;           // 1回のダメージ
	static inline const float kChannelMoveSpeedRate = 0.5f; // 火炎放射中の移動速度の倍率

	// 火炎球・炎の円・炎の見た目
	KamataEngine::Model* modelFire_ = nullptr;
	FireParticles* fireParticles_ = nullptr;
	std::list<Fireball*> fireballs_;
	std::list<FireField*> fireFields_;

	// 移動の入力方向（XZ平面の単位ベクトル。入力なしなら0）
	KamataEngine::Vector3 moveDirection_ = {};

	// 前転
	bool isRolling_ = false;
	KamataEngine::Vector3 rollDirection_ = {}; // 転がる方向
	int32_t rollTimer_ = 0;                    // 前転の残り時間<フレーム>
	int32_t rollCooldownTimer_ = 0;            // 次に前転できるまでの時間<フレーム>
	static inline const int32_t kRollTime = 18;        // 前転にかかる時間<フレーム>
	static inline const float kRollSpeed = 0.35f;      // 前転中の移動速度（18フレームで約6マス）
	static inline const int32_t kRollCooldown = 20;    // 前転後の再使用待ち<フレーム>

	// 弾
	KamataEngine::Model* modelBullet_ = nullptr;
	std::list<PlayerBullet*> bullets_;
	// 敵の指定
	const std::list<Enemy*>* enemies_ = nullptr;
	Enemy* targetEnemy_ = nullptr;  // 左クリックで指定した敵
	Enemy* hoveredEnemy_ = nullptr; // カーソルが乗っている敵
	// クリック判定を敵の当たり判定より少し大きくして選びやすくする
	static inline const float kPickPadding = 0.3f;

	// 狙っている方向（XZ平面の単位ベクトル）
	KamataEngine::Vector3 aimDirection_ = {0.0f, 0.0f, 1.0f};
	// 弾の速さ
	static inline const float kBulletSpeed = 0.5f;
	// 発射間隔<フレーム>
	static inline const int32_t kFireInterval = 10;
	int32_t fireTimer_ = 0;
};
