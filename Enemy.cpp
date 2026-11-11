#define NOMINMAX
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "Enemy.h"
#include "Player.h"
#include "MapChipField.h"
#include "calc.h"

using namespace KamataEngine;

void Enemy::Initialize(Model* model, Camera* camera, MapChipField* mapChipField, const Vector3& position) {
	assert(model);
	assert(mapChipField);

	model_ = model;
	camera_ = camera;

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
	mapChipField_ = mapChipField;

	// 右(+X)向き
	worldTransform_.rotation_.y = std::numbers::pi_v<float> / 2.0f;
	lrDirection_ = LRDirection::kRight;

	turnFirstRotationY_ = worldTransform_.rotation_.y;
	turnTimer_ = 0.0f;

	walkTimer_ = 0.0f;
}

void Enemy::Update() {
	// 自動移動の処理
	Move();

	// 衝突情報を初期化
	CollisionMapInfo collisionMapInfo;
	collisionMapInfo.move = velocity_;

	// マップとの衝突判定
	MapCollision(collisionMapInfo);

	// 旋回制御
	if (turnTimer_ > 0.0f) {
		turnTimer_ -= 1.0f / 60.0f;
		if (turnTimer_ < 0.0f) {
			turnTimer_ = 0.0f;
		}

		float destinationRotationYTable[] = {std::numbers::pi_v<float> / 2.0f, std::numbers::pi_v<float> * 3.0f / 2.0f};
		float destinationRotationY = destinationRotationYTable[static_cast<uint32_t>(lrDirection_)];

		float ratio = 1.0f - (turnTimer_ / kTimeTurn);
		worldTransform_.rotation_.y = std::lerp(turnFirstRotationY_, destinationRotationY, ratio);
	}

	// 衝突応答
	OnWallCollision(collisionMapInfo);

	// 左右の壁にヒットした場合は進行方向を反転させる自動巡回AI
	if (collisionMapInfo.hitWallX) {
		if (lrDirection_ == LRDirection::kRight) {
			lrDirection_ = LRDirection::kLeft;
		} else {
			lrDirection_ = LRDirection::kRight;
		}
		turnFirstRotationY_ = worldTransform_.rotation_.y;
		turnTimer_ = kTimeTurn;
		velocity_.x = 0.0f; // 反転時に一度速度をリセット
	}

	// 判定結果を反映して移動
	ApplyMove(collisionMapInfo);

	// 歩行アニメーション（X軸周りに前後に揺らす）
	walkTimer_ += 1.0f / 60.0f;
	float sinParam = std::sin(2.0f * std::numbers::pi_v<float> * walkTimer_);
	float ratio = (sinParam + 1.0f) / 2.0f;
	float degree = -kWalkMotionAngleStart + (kWalkMotionAngleStart + kWalkMotionAngleEnd) * ratio;
	worldTransform_.rotation_.x = degree * (std::numbers::pi_v<float> / 180.0f);

	// 行列更新
	worldTransform_.matWorld_ = calc::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();
}

void Enemy::Move() {
	// 現在の進行方向に向かって加速する自動移動（XZ平面上をX方向に往復）
	if (lrDirection_ == LRDirection::kRight) {
		velocity_.x += kAcceleration;
	} else {
		velocity_.x -= kAcceleration;
	}
	velocity_.x = std::clamp(velocity_.x, -kLimitRunSpeed, kLimitRunSpeed);
	velocity_.y = 0.0f;
	velocity_.z = 0.0f;
}

Vector3 Enemy::CornerPosition(const Vector3& center, Corner corner) {
	Vector3 offsetTable[kNumCorner] = {
	    {+kWidth / 2.0f, 0.0f, -kWidth / 2.0f}, // kRightNear
	    {-kWidth / 2.0f, 0.0f, -kWidth / 2.0f}, // kLeftNear
	    {+kWidth / 2.0f, 0.0f, +kWidth / 2.0f}, // kRightFar
	    {-kWidth / 2.0f, 0.0f, +kWidth / 2.0f}, // kLeftFar
	};

	const Vector3& offset = offsetTable[static_cast<uint32_t>(corner)];
	return {center.x + offset.x, center.y + offset.y, center.z + offset.z};
}

// マップ衝突判定の主処理
void Enemy::MapCollision(CollisionMapInfo& info) {
	MapCollisionFar(info);
	MapCollisionNear(info);
	MapCollisionRight(info);
	MapCollisionLeft(info);
}

// 奥(+Z)方向の当たり判定
void Enemy::MapCollisionFar(CollisionMapInfo& info) {
	if (info.move.z <= 0.0f) {
		return;
	}

	// 移動後の4つの角の座標
	std::array<Vector3, kNumCorner> positionsNew;
	for (uint32_t i = 0; i < positionsNew.size(); ++i) {
		positionsNew[i] = CornerPosition(calc::Add(worldTransform_.translation_, info.move), static_cast<Corner>(i));
	}

	MapChipType mapChipType;
	MapChipType mapChipTypeNext;
	bool hit = false;

	// 左奥点の判定（CSVでは +Z が上の行 = indexが小さい方向）
	MapChipField::IndexSet indexSet;
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kLeftFar]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y + 1);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	// 右奥点の判定
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kRightFar]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y + 1);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	if (hit) {
		const Vector3& pos = worldTransform_.translation_;
		MapChipField::IndexSet indexSetNow = mapChipField_->GetMapChipIndexByPosition({pos.x, 0.0f, pos.z + kWidth / 2.0f});
		indexSet = mapChipField_->GetMapChipIndexByPosition({pos.x, 0.0f, pos.z + info.move.z + kWidth / 2.0f});

		if (indexSetNow.y != indexSet.y) {
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.x, indexSet.y);
			float moveZ = rect.bottom - pos.z - (kWidth / 2.0f + kBlank);
			info.move.z = std::max(0.0f, moveZ);
			info.hitWallZ = true;
		}
	}
}

// 手前(-Z)方向の当たり判定
void Enemy::MapCollisionNear(CollisionMapInfo& info) {
	if (info.move.z >= 0.0f) {
		return;
	}

	std::array<Vector3, kNumCorner> positionsNew;
	for (uint32_t i = 0; i < positionsNew.size(); ++i) {
		positionsNew[i] = CornerPosition(calc::Add(worldTransform_.translation_, info.move), static_cast<Corner>(i));
	}

	MapChipType mapChipType;
	MapChipType mapChipTypeNext;
	bool hit = false;

	// 左手前点の判定
	MapChipField::IndexSet indexSet;
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kLeftNear]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y - 1);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	// 右手前点の判定
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kRightNear]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y - 1);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	if (hit) {
		const Vector3& pos = worldTransform_.translation_;
		MapChipField::IndexSet indexSetNow = mapChipField_->GetMapChipIndexByPosition({pos.x, 0.0f, pos.z - kWidth / 2.0f});
		indexSet = mapChipField_->GetMapChipIndexByPosition({pos.x, 0.0f, pos.z + info.move.z - kWidth / 2.0f});

		if (indexSetNow.y != indexSet.y) {
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.x, indexSet.y);
			float moveZ = rect.top + (kWidth / 2.0f + kBlank) - pos.z;
			info.move.z = std::min(0.0f, moveZ);
			info.hitWallZ = true;
		}
	}
}

// 右(+X)方向の当たり判定
void Enemy::MapCollisionRight(CollisionMapInfo& info) {
	if (info.move.x <= 0.0f) {
		return;
	}

	std::array<Vector3, kNumCorner> positionsNew;
	for (uint32_t i = 0; i < positionsNew.size(); ++i) {
		positionsNew[i] = CornerPosition(calc::Add(worldTransform_.translation_, info.move), static_cast<Corner>(i));
	}

	MapChipType mapChipType;
	MapChipType mapChipTypeNext;
	bool hit = false;

	// 右奥点の判定
	MapChipField::IndexSet indexSet;
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kRightFar]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.x - 1, indexSet.y);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	// 右手前点の判定
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kRightNear]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.x - 1, indexSet.y);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	if (hit) {
		const Vector3& pos = worldTransform_.translation_;
		MapChipField::IndexSet indexSetNow = mapChipField_->GetMapChipIndexByPosition({pos.x + kWidth / 2.0f, 0.0f, pos.z});
		indexSet = mapChipField_->GetMapChipIndexByPosition({pos.x + info.move.x + kWidth / 2.0f, 0.0f, pos.z + info.move.z});

		if (indexSetNow.x != indexSet.x) {
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.x, indexSet.y);
			float moveX = rect.left - (kWidth / 2.0f) - kBlank - pos.x;
			info.move.x = std::max(0.0f, moveX);
			info.hitWallX = true;
		}
	}
}

// 左(-X)方向の当たり判定
void Enemy::MapCollisionLeft(CollisionMapInfo& info) {
	if (info.move.x >= 0.0f) {
		return;
	}

	std::array<Vector3, kNumCorner> positionsNew;
	for (uint32_t i = 0; i < positionsNew.size(); ++i) {
		positionsNew[i] = CornerPosition(calc::Add(worldTransform_.translation_, info.move), static_cast<Corner>(i));
	}

	MapChipType mapChipType;
	MapChipType mapChipTypeNext;
	bool hit = false;

	// 左奥点の判定
	MapChipField::IndexSet indexSet;
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kLeftFar]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.x + 1, indexSet.y);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	// 左手前点の判定
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kLeftNear]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.x + 1, indexSet.y);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	if (hit) {
		const Vector3& pos = worldTransform_.translation_;
		MapChipField::IndexSet indexSetNow = mapChipField_->GetMapChipIndexByPosition({pos.x - kWidth / 2.0f, 0.0f, pos.z});
		indexSet = mapChipField_->GetMapChipIndexByPosition({pos.x + info.move.x - kWidth / 2.0f, 0.0f, pos.z + info.move.z});

		if (indexSetNow.x != indexSet.x) {
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.x, indexSet.y);
			float moveX = rect.right + (kWidth / 2.0f) + kBlank - pos.x;
			info.move.x = std::min(0.0f, moveX);
			info.hitWallX = true;
		}
	}
}

// 判定結果を反映して移動させる
void Enemy::ApplyMove(const CollisionMapInfo& info) {
	worldTransform_.translation_.x += info.move.x;
	worldTransform_.translation_.z += info.move.z;
}

// 壁に接触している場合の処理
void Enemy::OnWallCollision(const CollisionMapInfo& info) {
	if (info.hitWallX) {
		velocity_.x *= (1.0f - kAttenuationWall);
	}
	if (info.hitWallZ) {
		velocity_.z *= (1.0f - kAttenuationWall);
	}
}

// --- ワールド座標の取得 ---
Vector3 Enemy::GetWorldPosition() {
	Vector3 worldPos;
	worldPos.x = worldTransform_.matWorld_.m[3][0];
	worldPos.y = worldTransform_.matWorld_.m[3][1];
	worldPos.z = worldTransform_.matWorld_.m[3][2];
	return worldPos;
}

// --- AABBの計算と取得 ---
AABB Enemy::GetAABB() {
	Vector3 worldPos = GetWorldPosition();
	AABB aabb;
	aabb.min = {worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f};
	aabb.max = {worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f};
	return aabb;
}

// --- 衝突応答処理 ---
void Enemy::OnCollision(const Player* player) {
	(void)player;
}

void Enemy::OnBulletCollision() {
	// 弾が当たったら倒される
	isDead_ = true;
}

void Enemy::Draw() {
	model_->Draw(worldTransform_, *camera_);
}
