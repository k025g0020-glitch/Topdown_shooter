#pragma once
#include "KamataEngine.h"
#include "CollisionMapInfo.h"
#include <cassert>
#include <functional>
#include <list>

class MapChipField;
class Player;
class EnemyBullet;

class Enemy {
public:
	// 敵の種類
	enum class Type {
		kChaser,  // 近接型：自キャラに近づいてきて体当たり
		kShooter, // 遠距離型：距離をとりながら弾を撃ってくる
		kSwarm,   // 群れ型：体力は少ないが、大量に固まって襲ってくる
		kBoss,    // ボス：ボス部屋で待ち構え、複数の攻撃パターンを使う
	};

	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, MapChipField* mapChipField, const KamataEngine::Vector3& position, Type type);

	// 自キャラ（追いかける・狙う相手）を設定
	void SetPlayer(const Player* player) { player_ = player; }
	// 敵リスト（仲間と重ならないようにするため）を設定
	void SetEnemies(const std::list<Enemy*>* enemies) { enemies_ = enemies; }
	// 遠距離型が撃つ弾の追加先とモデルを設定
	void SetBulletList(std::list<EnemyBullet*>* bullets, KamataEngine::Model* bulletModel) {
		enemyBullets_ = bullets;
		modelBullet_ = bulletModel;
	}

	Type GetType() const { return type_; }

	// ---- ボス用 ----
	// ボス戦を開始する（眠っているボスが目を覚ます）
	void StartBossBattle();
	// 目を覚ましているか（眠っている間は攻撃してこず、ダメージも受けない）
	bool IsBossAwake() const { return type_ == Type::kBoss && bossState_ != BossState::kSleep; }
	int32_t GetMaxHp() const { return param_.maxHp; }
	// 手下を呼び出すときの生成処理を設定（GameScene が敵を生成する）
	void SetSpawnCallback(std::function<void(Type, const KamataEngine::Vector3&)> callback) { spawnCallback_ = callback; }

	// 所属する集団の番号を設定（-1 なら集団に属さない）
	void SetGroupId(int32_t groupId) { groupId_ = groupId; }
	int32_t GetGroupId() const { return groupId_; }

	// 自キャラに気づく。同じ集団の仲間にも知らせて、全員で向かってくる
	void BecomeAggro();
	void Update();
	void Draw();

	const KamataEngine::Vector3& GetTranslation() const { return worldTransform_.translation_; }
	void SetTranslation(const KamataEngine::Vector3& translation) { worldTransform_.translation_ = translation; }
	const KamataEngine::WorldTransform& GetWorldTransform() const { return worldTransform_; }
	const KamataEngine::Vector3& GetVelocity() const { return velocity_; }

	void SetMapChipField(MapChipField* mapChipField) { mapChipField_ = mapChipField; }

	KamataEngine::Vector3 GetWorldPosition() const;
	AABB GetAABB();

	void OnCollision(const Player* player);
	// ダメージを受ける（弾・スキル共通）
	void TakeDamage(int32_t damage);

	bool IsDead() const { return isDead_; }

	// カーソルが乗っている・狙われているときに明るく表示する
	void SetHighlighted(bool highlighted) { isHighlighted_ = highlighted; }
	int32_t GetHp() const { return hp_; }
	// 接触したときに自キャラに与えるダメージ
	int32_t GetContactDamage() const { return param_.contactDamage; }

private:
	// 上から見たときの4つの角（Far = 奥(+Z), Near = 手前(-Z)）
	enum Corner {
		kRightNear, // 右手前
		kLeftNear,  // 左手前
		kRightFar,  // 右奥
		kLeftFar,   // 左奥
		kNumCorner  // 要素数
	};

	// 種類ごとの行動（移動の目標速度を決める）
	void Think();
	// 弾を1発撃つ（direction: XZ平面の単位ベクトル）
	void FireBullet(const KamataEngine::Vector3& direction, float speed);
	// 攻撃の溜め中なら溜めの進み具合（0→1）、溜め中でなければ -1
	float GetChargeRatio() const;

	// ---- ボスの行動（EnemyBoss.cpp） ----
	void ThinkBoss();
	// 次の攻撃を選んで溜めを始める
	void StartNextBossAction();
	// 攻撃を終えて追いかけ状態に戻る
	void EndBossAction();
	// HPが半分以下になったら第2段階（攻撃が激しくなる）
	bool IsBossPhase2() const { return hp_ <= param_.maxHp / 2; }
	// 遠距離型の攻撃（溜めてから弾を撃つ）
	void UpdateShoot(float distanceToPlayer);
	// 仲間と重ならないよう離れる力
	KamataEngine::Vector3 CalcSeparation() const;

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

	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;

	KamataEngine::Vector3 velocity_ = {};
	MapChipField* mapChipField_ = nullptr;

	// 種類ごとのパラメータ
	struct Param {
		int32_t maxHp;          // 最大HP
		int32_t contactDamage;  // 接触ダメージ
		float speed;            // 最高速度
		float detectRange;      // 自キャラに気づく距離
		float scale;            // 見た目の大きさ
		float width;            // 当たり判定の幅・奥行き
		KamataEngine::Vector4 color; // 色
	};
	static Param GetParam(Type type);

	Type type_ = Type::kChaser;
	Param param_ = {};

	const Player* player_ = nullptr;
	const std::list<Enemy*>* enemies_ = nullptr;

	// 自キャラに気づいたか（一度気づいたら追い続ける。攻撃を受けても気づく）
	bool isAggro_ = false;
	// 所属する集団の番号
	int32_t groupId_ = -1;

	// 向きたい角度（Y軸回転）と旋回の補間率
	float targetRotationY_ = 0.0f;
	static inline const float kTurnRate = 0.2f;

	// 目標速度へ近づける割合（加速のなめらかさ）
	static inline const float kAccelRate = 0.15f;

	// 仲間と離れる処理の距離と強さ
	static inline const float kSeparationRadius = 0.9f;
	static inline const float kSeparationPower = 0.08f;

	// 遠距離型の攻撃
	std::list<EnemyBullet*>* enemyBullets_ = nullptr;
	KamataEngine::Model* modelBullet_ = nullptr;
	static inline const float kShooterNearDistance = 5.0f; // これより近いと離れる
	static inline const float kShooterFarDistance = 8.0f;  // これより遠いと近づく
	static inline const int32_t kShootInterval = 100;      // 撃つ間隔<フレーム>
	static inline const int32_t kShootChargeTime = 30;     // 撃つ前の溜め<フレーム>（この間は止まって光る）
	static inline const float kEnemyBulletSpeed = 0.18f;
	int32_t shootTimer_ = kShootInterval;

	// ---- ボス ----
	enum class BossState {
		kSleep,  // 眠っている（ボス戦開始まで）
		kWake,   // 目を覚ます演出
		kChase,  // ゆっくり追いかける
		kCharge, // 攻撃前の溜め（白く点滅）
		kRadial, // 全方位に弾をばらまく
		kFan,    // 自キャラに向けて扇状に弾を撃つ
		kDash,   // 自キャラに向かって突進
		kStun,   // 突進後のすき（攻撃のチャンス）
		kSummon, // 手下（群れ型）を呼び出す
	};
	BossState bossState_ = BossState::kSleep;
	BossState bossPendingAction_ = BossState::kRadial; // 溜めの後に行う攻撃
	int32_t bossTimer_ = 0;
	int32_t bossChargeDuration_ = 1; // 今回の溜めの長さ
	int32_t bossVolleyCount_ = 0;    // 何回目の発射か
	uint32_t bossActionIndex_ = 0;   // 攻撃の順番
	KamataEngine::Vector3 bossDashDirection_ = {};
	std::function<void(Type, const KamataEngine::Vector3&)> spawnCallback_;

	static inline const int32_t kBossWakeTime = 60;          // 目を覚ます演出<フレーム>
	static inline const int32_t kBossChaseTime = 90;         // 攻撃の間に追いかける時間<フレーム>
	static inline const int32_t kBossChaseTimePhase2 = 50;   // 第2段階
	static inline const int32_t kBossChargeTime = 45;        // 攻撃前の溜め<フレーム>
	static inline const int32_t kBossChargeTimePhase2 = 30;  // 第2段階
	static inline const int32_t kBossRadialBullets = 16;     // 全方位弾の数
	static inline const int32_t kBossRadialBulletsPhase2 = 24;
	static inline const int32_t kBossRadialVolleys = 3;      // 全方位弾を撃つ回数
	static inline const int32_t kBossRadialInterval = 20;    // 全方位弾の間隔<フレーム>
	static inline const int32_t kBossFanBullets = 5;         // 扇状弾の数
	static inline const float kBossFanSpread = 0.5f;         // 扇状弾の広がり（片側）<ラジアン>
	static inline const int32_t kBossFanVolleys = 3;         // 扇状弾を撃つ回数
	static inline const int32_t kBossFanInterval = 15;       // 扇状弾の間隔<フレーム>
	static inline const float kBossBulletSpeed = 0.14f;
	static inline const float kBossDashSpeed = 0.45f;        // 突進の速さ
	static inline const int32_t kBossDashTime = 30;          // 突進の最大時間<フレーム>
	static inline const int32_t kBossStunTime = 45;          // 突進後のすき<フレーム>
	static inline const int32_t kBossSummonCount = 6;        // 呼び出す手下の数
	static inline const float kBossSummonRadius = 2.5f;      // 手下を出す距離
	static inline const float kBossHitScale = 1.05f;         // 被弾時の膨らみ（大きいので控えめに）

	// 前のフレームで壁に当たったか（突進の停止に使う）
	bool lastHitWall_ = false;
	// 点滅などのアニメーション用カウンタ
	int32_t animFrame_ = 0;

	// 当たり判定サイズ
	float width_ = 0.8f;
	static inline const float kHeight = 0.8f; // 等倍のときの高さ
	float height_ = kHeight;
	static inline const float kBlank = 0.005f;

	// 壁接触時の速度減衰率
	static inline const float kAttenuationWall = 0.2f;

	// アニメーション
	static inline const float kWalkMotionAngleStart = 30.0f;
	static inline const float kWalkMotionAngleEnd = 30.0f;
	float walkTimer_ = 0.0f;

	bool isDead_ = false;

	// HP
	int32_t hp_ = 0;

	// 被弾演出（赤く点滅して少し膨らむ）
	static inline const int32_t kHitFlashTime = 10; // <フレーム>
	static inline const float kHitScale = 1.3f;     // 被弾した瞬間の拡大率
	int32_t hitTimer_ = 0;
	KamataEngine::ObjectColor objectColor_;

	// ハイライト表示
	bool isHighlighted_ = false;
};
