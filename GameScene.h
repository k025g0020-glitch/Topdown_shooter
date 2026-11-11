#pragma once
#include "CameraController.h"
#include "DeathParticles.h"
#include "Enemy.h"
#include "Fade.h"
#include "KamataEngine.h"
#include "MapChipField.h"
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
		kFadeOut, // フェードアウト
	};

	void Initialize();
	void Update();
	void Draw();
	void GenerateBlocks();
	void GenerateFloor();
	~GameScene();
	void CheckAllCollisions();
	void ChangePhase();
	bool IsFinished() const { return finished_; }

private:
	void UpdatePlayPhase();
	void UpdateDeathPhase();
	// ステージ（壁ブロック・床）の行列を更新
	void UpdateStage();

	static inline const float kFadeDuration = 1.0f;

	// 床の高さ: 床の上面がキャラの足元(y = -0.4)に来るように配置（cubeを0.5倍 → 高さ1）
	static inline const float kFloorY = -0.9f;
	// 壁ブロックの高さ: blockモデルは y = -0.27～0.73 なので、底面を床の上面に合わせる
	static inline const float kBlockY = -0.125f;

	Phase phase_ = Phase::kFadeIn;

	bool finished_ = false;

	std::vector<std::vector<KamataEngine::WorldTransform*>> worldTransformBlocks_;
	// 床タイル
	std::vector<KamataEngine::WorldTransform*> worldTransformFloors_;
	uint32_t textureHandle_ = 0;

	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Model* modelBlock_ = nullptr;
	KamataEngine::Model* modelFloor_ = nullptr;
	KamataEngine::Model* modelSkydome_ = nullptr;
	KamataEngine::Model* modelEnemy_ = nullptr;
	KamataEngine::Model* modelDeathParticle_ = nullptr;

	KamataEngine::Camera camera_;

	CameraController* cameraController_ = nullptr;
	Skydome* skydome_ = nullptr;
	MapChipField* mapChipField_ = nullptr;
	Player* player_ = nullptr;
	std::list<Enemy*> enemies_;
	DeathParticles* deathParticles_ = nullptr;
	Fade* fade_ = nullptr;

	bool isDebugCameraActive_ = false;
	KamataEngine::DebugCamera* debugCamera_ = nullptr;
};
