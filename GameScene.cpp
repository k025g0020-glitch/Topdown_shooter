#include "GameScene.h"

using namespace KamataEngine;

void GameScene::Initialize() {
	model_ = Model::CreateFromOBJ("player", true);
	modelEnemy_ = Model::CreateFromOBJ("enemy", true);
	modelBlock_ = Model::CreateFromOBJ("block", true);
	modelFloor_ = Model::CreateFromOBJ("cube", true);
	modelSkydome_ = Model::CreateFromOBJ("skydome", true);
	modelDeathParticle_ = Model::CreateFromOBJ("deathParticle", true);

	// カメラの初期化は CameraController::Initialize で行う
	debugCamera_ = new DebugCamera(1280, 720);

	skydome_ = new Skydome();
	skydome_->Initialize(modelSkydome_);

	mapChipField_ = new MapChipField;
	mapChipField_->LoadMapChipCsv("Resources/blocks.csv");

	// ステージ中央付近からスタート
	Vector3 playerInitialPosition = mapChipField_->GetMapChipPositionByIndex(15, 24);

	player_ = new Player();
	player_->Initialize(model_, &camera_, mapChipField_, playerInitialPosition);
	player_->SetMapChipField(mapChipField_);

	// 敵の配置（CSVの列, 行）
	const MapChipField::IndexSet enemyIndices[] = {
	    {5,  4 },
        {20, 9 },
        {8,  20}
    };
	for (const MapChipField::IndexSet& index : enemyIndices) {
		Enemy* newEnemy = new Enemy();
		Vector3 enemyPosition = mapChipField_->GetMapChipPositionByIndex(index.x, index.y);
		newEnemy->Initialize(modelEnemy_, &camera_, mapChipField_, enemyPosition);
		enemies_.push_back(newEnemy);
	}

	deathParticles_ = nullptr;

	cameraController_ = new CameraController();
	cameraController_->Initialize(&camera_);
	cameraController_->SetTarget(player_);
	// カメラの移動可能範囲（X: 左右, Z: 手前～奥）。カメラは自キャラより15手前にいる点に注意
	Rect area = {0.0f, 29.0f, -16.0f, 14.0f};
	cameraController_->SetMovableArea(area);
	cameraController_->Reset();

	GenerateBlocks();
	GenerateFloor();

	// 生成した直後にステージのワールド行列を計算＆定数バッファへ転送しておく
	UpdateStage();

	// カメラの初期状態を行列に反映
	cameraController_->Update();

	fade_ = new Fade();
	fade_->Initialize();
	fade_->Start(Fade::Status::FadeIn, kFadeDuration);

	phase_ = Phase::kFadeIn;
}

void GameScene::Update() {

	if (fade_) {
		fade_->Update();
	}

	ChangePhase();

	switch (phase_) {
	case Phase::kFadeIn:
		// 背景やキャラクターの更新を行う
		skydome_->Update();
		player_->Update();
		for (Enemy* enemy : enemies_) {
			enemy->Update();
		}

		if (!isDebugCameraActive_) {
			cameraController_->Update();
		}
		break;

	case Phase::kPlay:
		UpdatePlayPhase();
		break;

	case Phase::kDeath:
		UpdateDeathPhase();
		break;

	case Phase::kFadeOut:
		// デス演出完了後の背景等の更新
		skydome_->Update();
		for (Enemy* enemy : enemies_) {
			enemy->Update();
		}
		if (deathParticles_) {
			deathParticles_->Update();
		}
		break;
	}
}

void GameScene::ChangePhase() {
	switch (phase_) {
	case Phase::kFadeIn:
		if (fade_->IsFinished()) {
			fade_->Stop();
			phase_ = Phase::kPlay;
		}
		break;

	case Phase::kPlay:
		// 自キャラがデス状態か？
		if (player_->IsDead()) {
			// 死亡演出フェーズに切り替え
			phase_ = Phase::kDeath;

			// 自キャラの座標を取得
			const Vector3& deathParticlesPosition = player_->GetWorldPosition();

			// 自キャラの座標にデスパーティクルを発生、初期化
			deathParticles_ = new DeathParticles();
			deathParticles_->Initialize(modelDeathParticle_, &camera_, deathParticlesPosition);
		}
		break;

	case Phase::kDeath:
		// デスパーティクルが有効で演出が終了したらフェードアウト開始
		if (deathParticles_ && deathParticles_->IsFinished()) {
			phase_ = Phase::kFadeOut;
			fade_->Start(Fade::Status::FadeOut, kFadeDuration);
		}
		break;

	case Phase::kFadeOut:
		// フェードアウトが完了したらシーン終了
		if (fade_->IsFinished()) {
			finished_ = true;
		}
		break;
	}
}

// ゲームプレイフェーズの処理
void GameScene::UpdatePlayPhase() {
	// 天球の更新
	skydome_->Update();

	// 自キャラの更新
	player_->Update();

	// 敵の更新 (複数)
	for (Enemy* enemy : enemies_) {
		enemy->Update();
	}

	// カメラコントローラの更新＆カメラの更新
	if (!isDebugCameraActive_) {
		cameraController_->Update();
	}
	debugCamera_->Update();

#ifdef _DEBUG
	if (Input::GetInstance()->TriggerKey(DIK_RETURN)) {
		isDebugCameraActive_ = !isDebugCameraActive_;
	}
#endif

	if (isDebugCameraActive_) {
		debugCamera_->Update();
		camera_.matView = debugCamera_->GetCamera().matView;
		camera_.matProjection = debugCamera_->GetCamera().matProjection;
		camera_.TransferMatrix();
	}

	// ステージの更新
	UpdateStage();

	// 全ての当たり判定
	CheckAllCollisions();
}

// デス演出フェーズの処理
void GameScene::UpdateDeathPhase() {
	// 天球の更新
	skydome_->Update();

	// 敵の更新 (複数)
	for (Enemy* enemy : enemies_) {
		enemy->Update();
	}

	// デスパーティクルの更新
	if (deathParticles_) {
		deathParticles_->Update();
	}

	// カメラの更新
	debugCamera_->Update();

#ifdef _DEBUG
	if (Input::GetInstance()->TriggerKey(DIK_RETURN)) {
		isDebugCameraActive_ = !isDebugCameraActive_;
	}
#endif

	if (isDebugCameraActive_) {
		debugCamera_->Update();
		camera_.matView = debugCamera_->GetCamera().matView;
		camera_.matProjection = debugCamera_->GetCamera().matProjection;
		camera_.TransferMatrix();
	}

	// ステージの更新
	UpdateStage();
}

void GameScene::Draw() {
	Model::PreDraw();
	skydome_->Draw(camera_);

	if (phase_ == Phase::kPlay || phase_ == Phase::kFadeIn) {
		player_->Draw();
	}

	for (Enemy* enemy : enemies_) {
		enemy->Draw();
	}

	if (deathParticles_) {
		deathParticles_->Draw();
	}

	// 床の描画
	for (WorldTransform* worldTransformFloor : worldTransformFloors_) {
		modelFloor_->Draw(*worldTransformFloor, camera_);
	}

	// 壁ブロックの描画
	for (std::vector<WorldTransform*>& worldTransformBlockLine : worldTransformBlocks_) {
		for (WorldTransform* worldTransformBlock : worldTransformBlockLine) {
			if (!worldTransformBlock)
				continue;
			modelBlock_->Draw(*worldTransformBlock, camera_);
		}
	}
	Model::PostDraw();

	if (fade_) {
		fade_->Draw();
	}
}

void GameScene::GenerateBlocks() {
	// 要素数をマップチップから取得
	uint32_t numBlockVertical = mapChipField_->GetNumBlockVirtical();
	uint32_t numBlockHorizontal = mapChipField_->GetNumBlockHorizontal();

	// 列数を設定（縦方向のブロック数）
	worldTransformBlocks_.resize(numBlockVertical);
	for (uint32_t i = 0; i < numBlockVertical; ++i) {
		// 1列の要素数を設定（横方向のブロック数）
		worldTransformBlocks_[i].resize(numBlockHorizontal);
	}

	// ブロックの生成
	for (uint32_t i = 0; i < numBlockVertical; ++i) {
		for (uint32_t j = 0; j < numBlockHorizontal; ++j) {
			// マップチップデータに沿って配置する
			if (mapChipField_->GetMapChipTypeByIndex(j, i) == MapChipType::kBlock) {
				WorldTransform* worldTransform = new WorldTransform();
				worldTransform->Initialize();

				worldTransformBlocks_[i][j] = worldTransform;
				// 座標をマップチップから取得して設定（底面を床の高さに合わせる）
				worldTransformBlocks_[i][j]->translation_ = mapChipField_->GetMapChipPositionByIndex(j, i);
				worldTransformBlocks_[i][j]->translation_.y = kBlockY;
			} else {
				// 空きマスには nullptr を入れる
				worldTransformBlocks_[i][j] = nullptr;
			}
		}
	}
}

void GameScene::GenerateFloor() {
	uint32_t numBlockVertical = mapChipField_->GetNumBlockVirtical();
	uint32_t numBlockHorizontal = mapChipField_->GetNumBlockHorizontal();

	// 全マスに床タイルを敷く
	for (uint32_t i = 0; i < numBlockVertical; ++i) {
		for (uint32_t j = 0; j < numBlockHorizontal; ++j) {
			WorldTransform* worldTransform = new WorldTransform();
			worldTransform->Initialize();
			// cubeモデルは一辺2なので、0.5倍で1マス分のサイズにする
			worldTransform->scale_ = {0.5f, 0.5f, 0.5f};
			worldTransform->translation_ = mapChipField_->GetMapChipPositionByIndex(j, i);
			worldTransform->translation_.y = kFloorY;
			worldTransformFloors_.push_back(worldTransform);
		}
	}
}

void GameScene::UpdateStage() {
	// 壁ブロック
	for (std::vector<WorldTransform*>& worldTransformBlockLine : worldTransformBlocks_) {
		for (WorldTransform* worldTransformBlock : worldTransformBlockLine) {
			if (!worldTransformBlock)
				continue;
			worldTransformBlock->matWorld_ = calc::MakeAffineMatrix(worldTransformBlock->scale_, worldTransformBlock->rotation_, worldTransformBlock->translation_);
			worldTransformBlock->TransferMatrix();
		}
	}
	// 床
	for (WorldTransform* worldTransformFloor : worldTransformFloors_) {
		worldTransformFloor->matWorld_ = calc::MakeAffineMatrix(worldTransformFloor->scale_, worldTransformFloor->rotation_, worldTransformFloor->translation_);
		worldTransformFloor->TransferMatrix();
	}
}

void GameScene::CheckAllCollisions() {
#pragma region
	{
		// 判定対象1と2の座標
		AABB aabb1, aabb2;

		// 自キャラのAABB座標を取得
		aabb1 = player_->GetAABB();

		// 全ての敵キャラとの当たり判定
		for (Enemy* enemy : enemies_) {
			// 敵キャラのAABB座標を取得
			aabb2 = enemy->GetAABB();

			// AABB同士の交差判定（先ほど定義したIsCollisionを使用）
			if (calc::IsCollision(aabb1, aabb2)) {
				// 衝突応答
				// 自キャラの衝突時コールバックを呼び出す
				player_->OnCollision(enemy);
				// 敵キャラの衝突時コールバックを呼び出す
				enemy->OnCollision(player_);
			}
		}
	}
#pragma endregion
}

GameScene::~GameScene() {
	delete model_;
	delete player_;
	for (Enemy* enemy : enemies_) {
		delete enemy;
	}
	delete modelBlock_;
	delete skydome_;
	delete modelSkydome_;
	delete mapChipField_;
	delete cameraController_;
	if (deathParticles_) {
		delete deathParticles_;
	}
	for (std::vector<WorldTransform*>& worldTransformBlockLine : worldTransformBlocks_) {
		for (WorldTransform* worldTransformBlock : worldTransformBlockLine) {
			delete worldTransformBlock;
		}
	}
	worldTransformBlocks_.clear();
	for (WorldTransform* worldTransformFloor : worldTransformFloors_) {
		delete worldTransformFloor;
	}
	worldTransformFloors_.clear();
	delete modelFloor_;
	delete debugCamera_;

	if (fade_) {
		delete fade_;
	}
}