#include "GameScene.h"
#include <algorithm>

using namespace KamataEngine;

void GameScene::Initialize() {
	model_ = Model::CreateFromOBJ("player", true);
	modelEnemy_ = Model::CreateFromOBJ("enemy", true);
	modelBlock_ = Model::CreateFromOBJ("block", true);
	modelFloor_ = Model::CreateFromOBJ("cube", true);
	modelSkydome_ = Model::CreateFromOBJ("skydome", true);
	modelDeathParticle_ = Model::CreateFromOBJ("deathParticle", true);
	// 弾のモデル（小さな立方体のdeathParticleモデルを流用）
	modelBullet_ = Model::CreateFromOBJ("deathParticle", true);
	// 火炎球・炎のパーティクル用モデル（同じく deathParticle を色付けして使う）
	modelFire_ = Model::CreateFromOBJ("deathParticle", true);

	// カメラの初期化は CameraController::Initialize で行う
	debugCamera_ = new DebugCamera(1280, 720);

	skydome_ = new Skydome();
	skydome_->Initialize(modelSkydome_);

	mapChipField_ = new MapChipField;
	mapChipField_->LoadMapChipCsv("Resources/blocks.csv");

	// 自キャラ（位置は SpawnFromMap で CSV のスタート地点に置く）
	player_ = new Player();
	player_->Initialize(model_, &camera_, mapChipField_, {});
	player_->SetMapChipField(mapChipField_);
	player_->SetBulletModel(modelBullet_);
	// 敵の上で左クリックして狙えるよう、敵リストを渡す
	player_->SetEnemies(&enemies_);
	// スキルの初期化
	player_->InitializeSkills(modelFire_);

	// CSVの配置マーカーから、スタート地点・敵の集団・ボスを配置
	SpawnFromMap();

	// 扉のマスを記録しておく
	uint32_t numCols = mapChipField_->GetNumBlockHorizontal();
	uint32_t numRows = mapChipField_->GetNumBlockVirtical();
	isDoorCell_.assign(numCols * numRows, false);
	doorRow_ = numRows;
	for (uint32_t y = 0; y < numRows; ++y) {
		for (uint32_t x = 0; x < numCols; ++x) {
			if (mapChipField_->GetMapChipTypeByIndex(x, y) == MapChipType::kDoor) {
				doorCells_.push_back({x, y});
				isDoorCell_[y * numCols + x] = true;
				doorRow_ = std::min(doorRow_, y);
			}
		}
	}

	// ステージのタイルを用意（画面に映る範囲ぶんだけ）
	tilePool_.reserve(kTilePoolSize);
	for (uint32_t i = 0; i < kTilePoolSize; ++i) {
		WorldTransform* tile = new WorldTransform();
		tile->Initialize();
		tilePool_.push_back(tile);
	}
	tileModels_.assign(kTilePoolSize, nullptr);

	// ミニマップ・全体マップ
	miniMap_ = new MiniMap();
	miniMap_->Initialize(mapChipField_);

	deathParticles_ = nullptr;

	cameraController_ = new CameraController();
	cameraController_->Initialize(&camera_);
	cameraController_->SetTarget(player_);
	// カメラの移動可能範囲（X: 左右, Z: 手前～奥）。カメラは自キャラより15手前にいる点に注意
	// マップの大きさから決める
	float mapWidth = static_cast<float>(mapChipField_->GetNumBlockHorizontal());
	float mapDepth = static_cast<float>(mapChipField_->GetNumBlockVirtical());
	Rect area = {0.0f, mapWidth - 1.0f, -16.0f, mapDepth - 15.0f};
	cameraController_->SetMovableArea(area);
	cameraController_->Reset();

	// カメラの初期状態を行列に反映
	cameraController_->Update();

	// 画面に映るステージのタイルを並べる
	UpdateStage();

	// 自キャラのHPバー（画面左上）
	playerHpGauge_ = new HpGauge();
	playerHpGauge_->Initialize({40.0f, 40.0f}, {300.0f, 20.0f});
	playerHpGauge_->Update(static_cast<float>(player_->GetHp()), static_cast<float>(player_->GetMaxHp()));

	// 自キャラのMPバー（HPバーの下、青）
	playerMpGauge_ = new HpGauge();
	playerMpGauge_->Initialize({40.0f, 70.0f}, {300.0f, 14.0f}, {0.15f, 0.35f, 1.0f, 1.0f});
	playerMpGauge_->Update(player_->GetMp(), player_->GetMaxMp());

	// ボスのHPバー（画面上中央。ボス戦中だけ表示）
	bossHpGauge_ = new HpGauge();
	bossHpGauge_->Initialize({340.0f, 30.0f}, {600.0f, 18.0f}, {0.9f, 0.3f, 0.1f, 1.0f});

	// スキルアイコン（画面下中央）
	skillHud_ = new SkillHud();
	skillHud_->Initialize();
	skillHud_->Update(*player_);

	fade_ = new Fade();
	fade_->Initialize();
	fade_->Start(Fade::Status::FadeIn, kFadeDuration);

	phase_ = Phase::kFadeIn;
}

void GameScene::Update() {

	if (fade_) {
		fade_->Update();
	}

	// UIの更新
	playerHpGauge_->Update(static_cast<float>(player_->GetHp()), static_cast<float>(player_->GetMaxHp()));
	playerMpGauge_->Update(player_->GetMp(), player_->GetMaxMp());
	skillHud_->Update(*player_);
	if (boss_) {
		bossHpGauge_->Update(static_cast<float>(boss_->GetHp()), static_cast<float>(boss_->GetMaxHp()));
		miniMap_->SetBoss(true, boss_->GetTranslation());
	} else {
		miniMap_->SetBoss(false);
	}
	miniMap_->Update(player_->GetTranslation());

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
		UpdateStage();
		break;

	case Phase::kPlay:
	case Phase::kClear:
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
		UpdateEnemyDeathEffects();
		UpdateEnemyBullets();
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
		} else if (isBossDefeated_) {
			// ボスを倒したらクリア演出へ
			phase_ = Phase::kClear;
			clearTimer_ = kClearWaitTime;

			// 残っている敵も全滅させる（撃破エフェクトが次々に出る）
			for (Enemy* enemy : enemies_) {
				enemy->TakeDamage(enemy->GetMaxHp());
			}
			// 敵の弾も消す
			for (EnemyBullet* bullet : enemyBullets_) {
				delete bullet;
			}
			enemyBullets_.clear();
		}
		break;

	case Phase::kClear:
		// しばらく余韻を見せてからフェードアウト
		if (--clearTimer_ <= 0) {
			phase_ = Phase::kFadeOut;
			fade_->Start(Fade::Status::FadeOut, kFadeDuration);
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

	// カーソルが乗っている敵・狙っている敵をハイライト
	for (Enemy* enemy : enemies_) {
		enemy->SetHighlighted(enemy == player_->GetHoveredEnemy() || enemy == player_->GetTargetEnemy());
	}

	// 敵の更新 (複数)
	for (Enemy* enemy : enemies_) {
		enemy->Update();
	}

	// ボスが呼び出した手下を生成
	FlushPendingSpawns();

	// ボス部屋に入ったか
	CheckEnterBossRoom();

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

	// 敵の弾の更新
	UpdateEnemyBullets();

	// 全ての当たり判定
	CheckAllCollisions();

	// 倒された敵を削除し、その位置に撃破エフェクトを出す
	RemoveDeadEnemies();

	// 撃破エフェクトの更新
	UpdateEnemyDeathEffects();
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

	// 撃破エフェクトの更新
	UpdateEnemyDeathEffects();

	// 敵の弾の更新
	UpdateEnemyBullets();

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

	// 自キャラ（死亡後はデスパーティクルに置き換わる）
	if (!player_->IsDead()) {
		player_->Draw();
	}

	// 敵（画面に映る範囲だけ描く）
	for (Enemy* enemy : enemies_) {
		const Vector3& position = enemy->GetTranslation();
		if (IsInView(position.x, position.z, 3.0f)) {
			enemy->Draw();
		}
	}

	if (deathParticles_) {
		deathParticles_->Draw();
	}

	// 敵の弾の描画
	for (EnemyBullet* bullet : enemyBullets_) {
		bullet->Draw();
	}

	// 撃破エフェクトの描画
	for (EnemyDeathEffect* effect : enemyDeathEffects_) {
		effect->Draw();
	}

	// ステージ（画面に映る範囲の壁・床）の描画
	for (uint32_t i = 0; i < tileCount_; ++i) {
		tileModels_[i]->Draw(*tilePool_[i], camera_);
	}

	// 閉じた扉の描画
	for (WorldTransform* doorBlock : doorBlocks_) {
		modelBlock_->Draw(*doorBlock, camera_);
	}
	Model::PostDraw();

	// UIの描画
	playerHpGauge_->Draw();
	playerMpGauge_->Draw();
	skillHud_->Draw();
	// ボスのHPバー（ボス戦中のみ）
	if (isBossBattleStarted_ && boss_) {
		bossHpGauge_->Draw();
	}
	// ミニマップ・全体マップ
	miniMap_->Draw();

	if (fade_) {
		fade_->Draw();
	}
}

void GameScene::UpdateStage() {
	// 閉じた扉
	UpdateDoors();
	// 画面に映る範囲の壁・床
	UpdateVisibleTiles();
}

bool GameScene::IsInView(float x, float z, float margin) const {
	// 画面の中心（カメラが見ている地面の点）
	float centerX = camera_.translation_.x;
	float centerZ = camera_.translation_.z + 15.0f;
	float dz = z - centerZ;
	if (dz < -kViewNear - margin || dz > kViewFar + margin) {
		return false;
	}
	// 奥ほど横に広く映る（台形）
	float t = (dz + kViewNear) / (kViewNear + kViewFar);
	float halfWidth = kViewHalfWidthNear + (kViewHalfWidthFar - kViewHalfWidthNear) * std::clamp(t, 0.0f, 1.0f);
	return std::abs(x - centerX) <= halfWidth + margin;
}

void GameScene::UpdateVisibleTiles() {
	tileCount_ = 0;
	int32_t numCols = static_cast<int32_t>(mapChipField_->GetNumBlockHorizontal());
	int32_t numRows = static_cast<int32_t>(mapChipField_->GetNumBlockVirtical());

	// 画面の中心のマス
	float centerX = camera_.translation_.x;
	float centerZ = camera_.translation_.z + 15.0f;
	int32_t minCol = std::max(0, static_cast<int32_t>(centerX - kViewHalfWidthFar) - 1);
	int32_t maxCol = std::min(numCols - 1, static_cast<int32_t>(centerX + kViewHalfWidthFar) + 1);
	// Z が大きい（奥）ほど行番号は小さい
	int32_t minRow = std::max(0, numRows - 1 - static_cast<int32_t>(centerZ + kViewFar) - 1);
	int32_t maxRow = std::min(numRows - 1, numRows - 1 - static_cast<int32_t>(centerZ - kViewNear) + 1);

	// 手前の行から並べる（万一タイルが足りなくなっても、自キャラの近くは必ず描かれるように）
	for (int32_t y = maxRow; y >= minRow; --y) {
		for (int32_t x = minCol; x <= maxCol; ++x) {
			Vector3 position = mapChipField_->GetMapChipPositionByIndex(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
			if (!IsInView(position.x, position.z, 1.0f)) {
				continue;
			}
			if (tileCount_ >= kTilePoolSize) {
				return;
			}

			// 壁なら壁ブロック、それ以外（床・扉）なら床タイル
			// 閉じた扉は壁になっているが、せり上がる壁を別に描くので床を敷く
			bool isDoor = isDoorCell_[y * numCols + x];
			bool isWall = !isDoor && mapChipField_->GetMapChipTypeByIndex(static_cast<uint32_t>(x), static_cast<uint32_t>(y)) == MapChipType::kBlock;

			WorldTransform* tile = tilePool_[tileCount_];
			if (isWall) {
				tile->scale_ = {1.0f, 1.0f, 1.0f};
				position.y = kBlockY;
				tileModels_[tileCount_] = modelBlock_;
			} else {
				// cubeモデルは一辺2なので、0.5倍で1マス分のサイズにする
				tile->scale_ = {0.5f, 0.5f, 0.5f};
				position.y = kFloorY;
				tileModels_[tileCount_] = modelFloor_;
			}
			tile->translation_ = position;
			tile->matWorld_ = calc::MakeAffineMatrix(tile->scale_, tile->rotation_, tile->translation_);
			tile->TransferMatrix();
			++tileCount_;
		}
	}
}

void GameScene::SpawnFromMap() {
	// 先にスタート地点に自キャラを置く（敵が自キャラを参照するため）
	for (const MapChipField::SpawnPoint& spawn : mapChipField_->GetSpawnPoints()) {
		if (spawn.type == MapChipField::SpawnType::kPlayerStart) {
			player_->SetTranslation(mapChipField_->GetMapChipPositionByIndex(spawn.index.x, spawn.index.y));
		}
	}

	for (const MapChipField::SpawnPoint& spawn : mapChipField_->GetSpawnPoints()) {
		uint32_t x = spawn.index.x;
		uint32_t y = spawn.index.y;
		switch (spawn.type) {
		case MapChipField::SpawnType::kChaserGroup:
			SpawnGroup(Enemy::Type::kChaser, x, y, kGroupSize);
			break;
		case MapChipField::SpawnType::kShooterGroup:
			SpawnGroup(Enemy::Type::kShooter, x, y, kGroupSize);
			break;
		case MapChipField::SpawnType::kSwarmGroup:
			SpawnGroup(Enemy::Type::kSwarm, x, y, kSwarmGroupSize);
			break;
		case MapChipField::SpawnType::kBoss:
			boss_ = SpawnEnemy(Enemy::Type::kBoss, x, y);
			// ボスが手下を呼び出したら、あとでまとめて生成する
			boss_->SetSpawnCallback([this](Enemy::Type type, const Vector3& position) { pendingSpawns_.push_back({type, position}); });
			break;
		case MapChipField::SpawnType::kPlayerStart:
			break;
		}
	}
}

Enemy* GameScene::SpawnEnemy(Enemy::Type type, uint32_t xIndex, uint32_t yIndex, int32_t groupId) {
	return SpawnEnemyAt(type, mapChipField_->GetMapChipPositionByIndex(xIndex, yIndex), groupId);
}

Enemy* GameScene::SpawnEnemyAt(Enemy::Type type, const Vector3& position, int32_t groupId) {
	Enemy* newEnemy = new Enemy();
	newEnemy->Initialize(modelEnemy_, &camera_, mapChipField_, position, type);
	newEnemy->SetPlayer(player_);
	newEnemy->SetEnemies(&enemies_);
	newEnemy->SetBulletList(&enemyBullets_, modelBullet_);
	newEnemy->SetGroupId(groupId);
	enemies_.push_back(newEnemy);
	return newEnemy;
}

void GameScene::FlushPendingSpawns() {
	for (const PendingSpawn& spawn : pendingSpawns_) {
		// 壁の中には出さない
		MapChipField::IndexSet index = mapChipField_->GetMapChipIndexByPosition(spawn.position);
		if (mapChipField_->GetMapChipTypeByIndex(index.x, index.y) == MapChipType::kBlock) {
			continue;
		}
		Enemy* minion = SpawnEnemyAt(spawn.type, spawn.position);
		// 呼び出された手下は最初から自キャラに向かってくる
		minion->BecomeAggro();
	}
	pendingSpawns_.clear();
}

void GameScene::RemoveDeadEnemies() {
	enemies_.remove_if([this](Enemy* enemy) {
		if (!enemy->IsDead()) {
			return false;
		}

		Vector3 position = enemy->GetWorldPosition();
		EnemyDeathEffect* effect = new EnemyDeathEffect();
		effect->Initialize(modelDeathParticle_, &camera_, position);
		enemyDeathEffects_.push_back(effect);

		// ボスを倒した：周りにも撃破エフェクトを出して派手にする
		if (enemy == boss_) {
			const Vector3 offsets[] = {{1.5f, 0.0f, 0.0f}, {-1.5f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.5f}, {0.0f, 0.0f, -1.5f}, {0.0f, 1.5f, 0.0f}};
			for (const Vector3& offset : offsets) {
				EnemyDeathEffect* bigEffect = new EnemyDeathEffect();
				bigEffect->Initialize(modelDeathParticle_, &camera_, calc::Add(position, offset));
				enemyDeathEffects_.push_back(bigEffect);
			}
			boss_ = nullptr;
			isBossDefeated_ = true;
		}

		delete enemy;
		return true;
	});
}

void GameScene::CheckEnterBossRoom() {
	if (isBossBattleStarted_ || !boss_) {
		return;
	}
	// 自キャラがボス部屋の床にいて、扉より2マス以上奥に入ったらボス戦開始
	MapChipField::IndexSet index = mapChipField_->GetMapChipIndexByPosition(player_->GetTranslation());
	if (mapChipField_->IsBossRoomByIndex(index.x, index.y) && index.y + kBossRoomEnterDepth <= doorRow_) {
		StartBossBattle();
	}
}

void GameScene::StartBossBattle() {
	isBossBattleStarted_ = true;

	// 扉を閉じる：マップ上は壁にし、見た目は床の下から壁をせり上げる
	for (const MapChipField::IndexSet& cell : doorCells_) {
		mapChipField_->SetMapChipTypeByIndex(cell.x, cell.y, MapChipType::kBlock);

		WorldTransform* doorBlock = new WorldTransform();
		doorBlock->Initialize();
		doorBlock->translation_ = mapChipField_->GetMapChipPositionByIndex(cell.x, cell.y);
		doorBlock->translation_.y = kBlockY + kDoorStartOffsetY;
		doorBlocks_.push_back(doorBlock);
	}
	doorRiseTimer_ = kDoorRiseTime;

	// ボスを起こす
	boss_->StartBossBattle();
}

void GameScene::UpdateDoors() {
	if (doorRiseTimer_ > 0) {
		--doorRiseTimer_;
	}
	// 経過割合 0 → 1（最後はゆっくり止まる）
	float t = 1.0f - static_cast<float>(doorRiseTimer_) / static_cast<float>(kDoorRiseTime);
	float easeOut = 1.0f - (1.0f - t) * (1.0f - t);

	for (WorldTransform* doorBlock : doorBlocks_) {
		doorBlock->translation_.y = kBlockY + kDoorStartOffsetY * (1.0f - easeOut);
		doorBlock->matWorld_ = calc::MakeAffineMatrix(doorBlock->scale_, doorBlock->rotation_, doorBlock->translation_);
		doorBlock->TransferMatrix();
	}
}

void GameScene::SpawnGroup(Enemy::Type type, uint32_t xIndex, uint32_t yIndex, uint32_t count) {
	// 中心から縦横3マス以内のマスを候補にする
	const int32_t kSearchRange = 3;
	struct Cell {
		uint32_t x;
		uint32_t y;
		int32_t distanceSq; // 中心からの距離の2乗
	};
	std::vector<Cell> cells;
	for (int32_t dy = -kSearchRange; dy <= kSearchRange; ++dy) {
		for (int32_t dx = -kSearchRange; dx <= kSearchRange; ++dx) {
			int32_t x = static_cast<int32_t>(xIndex) + dx;
			int32_t y = static_cast<int32_t>(yIndex) + dy;
			// マップ外と壁のマスには置かない
			if (x < 0 || y < 0 || x >= static_cast<int32_t>(mapChipField_->GetNumBlockHorizontal()) || y >= static_cast<int32_t>(mapChipField_->GetNumBlockVirtical())) {
				continue;
			}
			if (mapChipField_->GetMapChipTypeByIndex(static_cast<uint32_t>(x), static_cast<uint32_t>(y)) == MapChipType::kBlock) {
				continue;
			}
			cells.push_back({static_cast<uint32_t>(x), static_cast<uint32_t>(y), dx * dx + dy * dy});
		}
	}

	// この集団の番号（1体が気づいたら同じ番号の仲間も一斉に気づく）
	int32_t groupId = nextGroupId_++;

	// 中心に近い順に並べ、先頭から count 体ぶん配置する
	std::stable_sort(cells.begin(), cells.end(), [](const Cell& a, const Cell& b) { return a.distanceSq < b.distanceSq; });
	for (uint32_t i = 0; i < count && i < cells.size(); ++i) {
		SpawnEnemy(type, cells[i].x, cells[i].y, groupId);
	}
}

void GameScene::UpdateEnemyBullets() {
	for (EnemyBullet* bullet : enemyBullets_) {
		bullet->Update();
	}
	enemyBullets_.remove_if([](EnemyBullet* bullet) {
		if (bullet->IsDead()) {
			delete bullet;
			return true;
		}
		return false;
	});
}

void GameScene::UpdateEnemyDeathEffects() {
	for (EnemyDeathEffect* effect : enemyDeathEffects_) {
		effect->Update();
	}
	// 演出が終わったエフェクトを削除
	enemyDeathEffects_.remove_if([](EnemyDeathEffect* effect) {
		if (effect->IsFinished()) {
			delete effect;
			return true;
		}
		return false;
	});
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

#pragma region 敵の弾と自キャラの当たり判定
	{
		AABB aabbPlayer = player_->GetAABB();
		for (EnemyBullet* bullet : enemyBullets_) {
			if (bullet->IsDead()) {
				continue;
			}
			// 前転中は弾をすり抜ける（回避）
			if (player_->IsRolling()) {
				break;
			}
			if (calc::IsCollision(aabbPlayer, bullet->GetAABB())) {
				bullet->OnCollision();
				player_->TakeDamage(bullet->GetDamage());
			}
		}
	}
#pragma endregion

#pragma region 自弾と敵キャラの当たり判定
	for (PlayerBullet* bullet : player_->GetBullets()) {
		if (bullet->IsDead()) {
			continue;
		}
		AABB aabbBullet = bullet->GetAABB();

		for (Enemy* enemy : enemies_) {
			if (enemy->IsDead()) {
				continue;
			}
			if (calc::IsCollision(aabbBullet, enemy->GetAABB())) {
				// 弾は消え、敵は倒される
				bullet->OnCollision();
				enemy->TakeDamage(bullet->GetDamage());
				// 1発の弾で倒せるのは1体だけ
				break;
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
	for (EnemyDeathEffect* effect : enemyDeathEffects_) {
		delete effect;
	}
	enemyDeathEffects_.clear();
	for (EnemyBullet* bullet : enemyBullets_) {
		delete bullet;
	}
	enemyBullets_.clear();
	for (WorldTransform* tile : tilePool_) {
		delete tile;
	}
	tilePool_.clear();
	delete miniMap_;
	delete modelFloor_;
	delete modelBullet_;
	delete debugCamera_;

	if (fade_) {
		delete fade_;
	}
	delete playerHpGauge_;
	delete bossHpGauge_;
	for (WorldTransform* doorBlock : doorBlocks_) {
		delete doorBlock;
	}
	doorBlocks_.clear();
	delete playerMpGauge_;
	delete skillHud_;
	delete modelFire_;
}