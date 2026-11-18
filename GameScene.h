#pragma once
#include "CameraController.h"
#include "DeathParticles.h"
#include "Enemy.h"
#include "EnemyDeathEffect.h"
#include "EnemyBullet.h"
#include "Fade.h"
#include "HpGauge.h"
#include "SkillHud.h"
#include "KamataEngine.h"
#include "MapChipField.h"
#include "MiniMap.h"
#include "Player.h"
#include "Skydome.h"
#include "calc.h"
#include <list>
#include <vector>

class GameScene {
public:
	enum class Phase {
		kFadeIn,  // フェードイン
		kPlay,    // ゲームプレイ
		kDeath,   // デス演出
		kClear,   // ボス撃破後のクリア演出
		kFadeOut, // フェードアウト
	};

	void Initialize();
	void Update();
	void Draw();
	~GameScene();
	void CheckAllCollisions();
	void ChangePhase();
	bool IsFinished() const { return finished_; }

private:
	void UpdatePlayPhase();
	void UpdateDeathPhase();
	// ステージ（壁ブロック・床・扉）の更新
	void UpdateStage();
	// 画面に映る範囲のマスだけ、タイル（壁・床）を並べ直す
	void UpdateVisibleTiles();
	// 画面に映る範囲か（x, z: ワールド座標）。描画を省くのに使う
	bool IsInView(float x, float z, float margin) const;
	// CSVの配置マーカーから自キャラ・敵・ボスを配置する
	void SpawnFromMap();
	// 撃破エフェクトの更新（終わったものは削除）
	void UpdateEnemyDeathEffects();
	// 敵の生成（種類と位置を指定）
	// groupId: 所属する集団の番号（-1 なら単独）
	Enemy* SpawnEnemy(Enemy::Type type, uint32_t xIndex, uint32_t yIndex, int32_t groupId = -1);
	// 敵の生成（ワールド座標で位置を指定）
	Enemy* SpawnEnemyAt(Enemy::Type type, const KamataEngine::Vector3& position, int32_t groupId = -1);
	// ボスが呼び出した手下を実際に生成する（敵の更新ループの外で行う）
	void FlushPendingSpawns();
	// 倒された敵を削除し、撃破エフェクトを出す
	void RemoveDeadEnemies();

	// ---- ボス部屋 ----
	// 自キャラがボス部屋に入ったか調べ、入ったらボス戦を始める
	void CheckEnterBossRoom();
	// ボス戦開始：扉を閉じてボスを起こす
	void StartBossBattle();
	// 閉じた扉（せり上がる壁）の更新
	void UpdateDoors();
	// 敵を集団で生成（中心のマスから近い順に、壁以外のマスへ count 体配置）
	void SpawnGroup(Enemy::Type type, uint32_t xIndex, uint32_t yIndex, uint32_t count);
	// 敵の弾の更新（消えたものは削除）
	void UpdateEnemyBullets();

	static inline const float kFadeDuration = 1.0f;

	// 1集団あたりの敵の数
	static inline const uint32_t kGroupSize = 5;       // 近接型・遠距離型
	static inline const uint32_t kSwarmGroupSize = 15; // 群れ型
	// 次に作る集団の番号
	int32_t nextGroupId_ = 0;

	// ボス
	Enemy* boss_ = nullptr;          // 倒されたら nullptr
	bool isBossBattleStarted_ = false;
	bool isBossDefeated_ = false;
	HpGauge* bossHpGauge_ = nullptr;
	// ボス部屋の床で、扉の行より2マス以上奥に入ったらボス戦開始
	static inline const uint32_t kBossRoomEnterDepth = 2;
	uint32_t doorRow_ = 0;

	// ボスが呼び出した手下（あとでまとめて生成する）
	struct PendingSpawn {
		Enemy::Type type;
		KamataEngine::Vector3 position;
	};
	std::vector<PendingSpawn> pendingSpawns_;

	// 扉
	std::vector<MapChipField::IndexSet> doorCells_;           // 扉のマス
	std::vector<bool> isDoorCell_;                            // 扉のマスか [行 * 列数 + 列]
	std::vector<KamataEngine::WorldTransform*> doorBlocks_;   // 閉じた扉の壁
	int32_t doorRiseTimer_ = 0;
	static inline const int32_t kDoorRiseTime = 30;           // 壁がせり上がる時間<フレーム>
	static inline const float kDoorStartOffsetY = -1.2f;      // せり上がる前の高さ（床の下）

	// クリア演出
	int32_t clearTimer_ = 0;
	static inline const int32_t kClearWaitTime = 150;         // ボス撃破からフェードアウトまで<フレーム>

	// 床の高さ: 床の上面がキャラの足元(y = -0.4)に来るように配置（cubeを0.5倍 → 高さ1）
	static inline const float kFloorY = -0.9f;
	// 壁ブロックの高さ: blockモデルは y = -0.27～0.73 なので、底面を床の上面に合わせる
	static inline const float kBlockY = -0.125f;

	Phase phase_ = Phase::kFadeIn;

	bool finished_ = false;

	// ステージのタイル（壁・床）
	// マップ全体ぶん作ると重いので、画面に映る範囲のぶんだけ用意して毎フレーム並べ直す
	std::vector<KamataEngine::WorldTransform*> tilePool_;
	std::vector<KamataEngine::Model*> tileModels_; // 各タイルに使うモデル（壁 or 床）
	uint32_t tileCount_ = 0;                        // 今フレーム使うタイルの数
	static inline const uint32_t kTilePoolSize = 2200;
	// タイルを並べる範囲（画面の中心からの距離<マス>）。奥ほど横に広く映るので台形にする
	// 実際に画面に映る範囲（手前 約10マス・横幅の半分 約16.5、奥 約19マス・横幅の半分 約30）より
	// 3マスほど広くとり、カメラの動きや壁の高さがあっても端が欠けないようにしている
	static inline const float kViewNear = 13.0f;          // 手前
	static inline const float kViewFar = 23.0f;           // 奥
	static inline const float kViewHalfWidthNear = 18.0f; // 手前の端での横幅の半分
	static inline const float kViewHalfWidthFar = 34.0f;  // 奥の端での横幅の半分
	// ミニマップ・全体マップ
	MiniMap* miniMap_ = nullptr;

	uint32_t textureHandle_ = 0;

	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Model* modelBlock_ = nullptr;
	KamataEngine::Model* modelFloor_ = nullptr;
	KamataEngine::Model* modelSkydome_ = nullptr;
	KamataEngine::Model* modelEnemy_ = nullptr;
	KamataEngine::Model* modelDeathParticle_ = nullptr;
	KamataEngine::Model* modelBullet_ = nullptr;

	KamataEngine::Camera camera_;

	CameraController* cameraController_ = nullptr;
	Skydome* skydome_ = nullptr;
	MapChipField* mapChipField_ = nullptr;
	Player* player_ = nullptr;
	std::list<Enemy*> enemies_;
	DeathParticles* deathParticles_ = nullptr;
	// 敵の弾
	std::list<EnemyBullet*> enemyBullets_;
	// 敵の撃破エフェクト
	std::list<EnemyDeathEffect*> enemyDeathEffects_;
	Fade* fade_ = nullptr;
	// 自キャラのHPバー
	HpGauge* playerHpGauge_ = nullptr;
	// 自キャラのMPバー
	HpGauge* playerMpGauge_ = nullptr;
	// スキルアイコン
	SkillHud* skillHud_ = nullptr;
	// 火炎球・炎のパーティクル用モデル
	KamataEngine::Model* modelFire_ = nullptr;

	bool isDebugCameraActive_ = false;
	KamataEngine::DebugCamera* debugCamera_ = nullptr;
};
