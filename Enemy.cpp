#define NOMINMAX
#include <algorithm>
#include <array>
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

	worldTransform_.rotation_.y = std::numbers::pi_v<float> / 2.0f;
	lrDirection_ = LRDirection::kRight;

	turnFirstRotationY_ = worldTransform_.rotation_.y;
	turnTimer_ = 0.0f;

	onGround_ = true;

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
	OnCeilingCollision(collisionMapInfo);
	OnWallCollision(collisionMapInfo);

	// 壁にヒットした場合は進行方向を反転させる自動巡回AI
	if (collisionMapInfo.hitWall) {
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

	// 接地状態切り替え
	OnGroundToggle(collisionMapInfo);

	walkTimer_ += 1.0f / 60.0f;

	// 1. サイン波の周期を計算（2*π をかけて1秒で1周期にするなど調整）
	float sinParam = std::sin(2.0f * std::numbers::pi_v<float> * walkTimer_);

	// 2. 算出した数値を -1.0f ~ +1.0f から 0.0f ~ 1.0f の係数 t (ratio) に加工する
	float ratio = (sinParam + 1.0f) / 2.0f;

	// 3. 最初の角度と最後の角度を使って線形補間（degree）
	// ※Enemy.hで定義されている -30度 から +30度 の範囲で往復させたい場合は、初期値をマイナスにします
	float degree = -kWalkMotionAngleStart + (kWalkMotionAngleStart + kWalkMotionAngleEnd) * ratio;

	// 4. 度（degree）をラジアンに変換して X軸周りの角度に代入
	worldTransform_.rotation_.x = degree * (std::numbers::pi_v<float> / 180.0f);

	// 行列更新
	worldTransform_.matWorld_ = calc::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();
}

void Enemy::Move() {
	Vector3 acceleration = {};

	if (onGround_) {
		// 現在の進行方向に向かって加速する自動移動
		if (lrDirection_ == LRDirection::kRight) {
			acceleration.x += kAcceleration;
		} else if (lrDirection_ == LRDirection::kLeft) {
			acceleration.x -= kAcceleration;
		}

		velocity_.x += acceleration.x;
		velocity_.x = std::clamp(velocity_.x, -kLimitRunSpeed, kLimitRunSpeed);

	} else {
		// 空中（重力の加算）
		velocity_.y -= kGravityAcceleration;
		velocity_.y = std::max(velocity_.y, -kLimitFallSpeed);
	}
}

Vector3 Enemy::CornerPosition(const Vector3& center, Corner corner) {
	Vector3 offsetTable[kNumCorner] = {
	    {+kWidth / 2.0f, -kHeight / 2.0f, 0.0f}, // kRightBottom
	    {-kWidth / 2.0f, -kHeight / 2.0f, 0.0f}, // kLeftBottom
	    {+kWidth / 2.0f, +kHeight / 2.0f, 0.0f}, // kRightTop
	    {-kWidth / 2.0f, +kHeight / 2.0f, 0.0f}, // kLeftTop
	};

	return {center.x + offsetTable[static_cast<uint32_t>(corner)].x, center.y + offsetTable[static_cast<uint32_t>(corner)].y, center.z + offsetTable[static_cast<uint32_t>(corner)].z};
}

void Enemy::MapCollision(CollisionMapInfo& info) {
	MapCollisionUp(info);
	MapCollisionDown(info);
	MapCollisionRight(info);
	MapCollisionLeft(info);
}

void Enemy::MapCollisionUp(CollisionMapInfo& info) {
	if (info.move.y <= 0.0f)
		return;

	std::array<Vector3, kNumCorner> positionsNew;
	for (uint32_t i = 0; i < positionsNew.size(); ++i) {
		Vector3 targetPos = {worldTransform_.translation_.x + info.move.x, worldTransform_.translation_.y + info.move.y, worldTransform_.translation_.z + info.move.z};
		positionsNew[i] = CornerPosition(targetPos, static_cast<Corner>(i));
	}

	MapChipType mapChipType;
	MapChipType mapChipTypeNext;
	bool hit = false;

	MapChipField::IndexSet indexSet;
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kLeftTop]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y + 1);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kRightTop]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y + 1);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	if (hit) {
		Vector3 targetTopPosBefore = {worldTransform_.translation_.x, worldTransform_.translation_.y + kHeight / 2.0f, 0.0f};
		MapChipField::IndexSet indexSetNow = mapChipField_->GetMapChipIndexByPosition(targetTopPosBefore);
		indexSet = mapChipField_->GetMapChipIndexByPosition(Vector3(worldTransform_.translation_.x, worldTransform_.translation_.y + info.move.y + kHeight / 2.0f, 0.0f));

		if (indexSetNow.y != indexSet.y) {
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.x, indexSet.y);
			float blueArrow = rect.bottom - worldTransform_.translation_.y;
			float greenArrow = (kHeight / 2.0f) + kBlank;
			float moveY = blueArrow - greenArrow;

			info.move.y = std::max(0.0f, moveY);
			info.ceilingCollision = true;
		}
	}
}

void Enemy::MapCollisionDown(CollisionMapInfo& info) {
	if (info.move.y >= 0.0f)
		return;

	std::array<Vector3, kNumCorner> positionsNew;
	for (uint32_t i = 0; i < positionsNew.size(); ++i) {
		Vector3 targetPos = {worldTransform_.translation_.x + info.move.x, worldTransform_.translation_.y + info.move.y, worldTransform_.translation_.z + info.move.z};
		positionsNew[i] = CornerPosition(targetPos, static_cast<Corner>(i));
	}

	MapChipType mapChipType;
	MapChipType mapChipTypeNext;
	bool hit = false;

	MapChipField::IndexSet indexSet;
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kLeftBottom]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y - 1);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kRightBottom]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y - 1);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	if (hit) {
		Vector3 targetBottomPosBefore = {worldTransform_.translation_.x, worldTransform_.translation_.y - kHeight / 2.0f, 0.0f};
		MapChipField::IndexSet indexSetNow = mapChipField_->GetMapChipIndexByPosition(targetBottomPosBefore);
		Vector3 targetBottomPos = {worldTransform_.translation_.x + info.move.x, worldTransform_.translation_.y + info.move.y - kHeight / 2.0f, 0.0f};
		indexSet = mapChipField_->GetMapChipIndexByPosition(targetBottomPos);

		if (indexSetNow.y != indexSet.y) {
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.x, indexSet.y);
			float moveY = rect.top + (kHeight / 2.0f) + kBlank - worldTransform_.translation_.y;
			info.move.y = std::min(0.0f, moveY);
			info.landing = true;
		}
	}
}

void Enemy::MapCollisionRight(CollisionMapInfo& info) {
	if (info.move.x <= 0.0f)
		return;

	std::array<Vector3, kNumCorner> positionsNew;
	for (uint32_t i = 0; i < positionsNew.size(); ++i) {
		Vector3 targetPos = {worldTransform_.translation_.x + info.move.x, worldTransform_.translation_.y + info.move.y, worldTransform_.translation_.z + info.move.z};
		positionsNew[i] = CornerPosition(targetPos, static_cast<Corner>(i));
	}

	MapChipType mapChipType;
	MapChipType mapChipTypeNext;
	bool hit = false;

	MapChipField::IndexSet indexSet;
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kRightTop]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.x - 1, indexSet.y);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kRightBottom]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.x - 1, indexSet.y);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	if (hit) {
		Vector3 targetRightPosBefore = {worldTransform_.translation_.x + kWidth / 2.0f, worldTransform_.translation_.y, 0.0f};
		MapChipField::IndexSet indexSetNow = mapChipField_->GetMapChipIndexByPosition(targetRightPosBefore);
		Vector3 targetRightPos = {worldTransform_.translation_.x + info.move.x + kWidth / 2.0f, worldTransform_.translation_.y + info.move.y, 0.0f};
		indexSet = mapChipField_->GetMapChipIndexByPosition(targetRightPos);

		if (indexSetNow.x != indexSet.x) {
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.x, indexSet.y);
			float moveX = rect.left - (kWidth / 2.0f) - kBlank - worldTransform_.translation_.x;
			info.move.x = std::max(0.0f, moveX);
			info.hitWall = true;
		}
	}
}

void Enemy::MapCollisionLeft(CollisionMapInfo& info) {
	if (info.move.x >= 0.0f)
		return;

	std::array<Vector3, kNumCorner> positionsNew;
	for (uint32_t i = 0; i < positionsNew.size(); ++i) {
		Vector3 targetPos = {worldTransform_.translation_.x + info.move.x, worldTransform_.translation_.y + info.move.y, worldTransform_.translation_.z + info.move.z};
		positionsNew[i] = CornerPosition(targetPos, static_cast<Corner>(i));
	}

	MapChipType mapChipType;
	MapChipType mapChipTypeNext;
	bool hit = false;

	MapChipField::IndexSet indexSet;
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kLeftTop]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.x + 1, indexSet.y);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kLeftBottom]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.x + 1, indexSet.y);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	if (hit) {
		Vector3 targetLeftPosBefore = {worldTransform_.translation_.x - kWidth / 2.0f, worldTransform_.translation_.y, 0.0f};
		MapChipField::IndexSet indexSetNow = mapChipField_->GetMapChipIndexByPosition(targetLeftPosBefore);
		Vector3 targetLeftPos = {worldTransform_.translation_.x + info.move.x - kWidth / 2.0f, worldTransform_.translation_.y + info.move.y, 0.0f};
		indexSet = mapChipField_->GetMapChipIndexByPosition(targetLeftPos);

		if (indexSetNow.x != indexSet.x) {
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.x, indexSet.y);
			float moveX = rect.right + (kWidth / 2.0f) + kBlank - worldTransform_.translation_.x;
			info.move.x = std::min(0.0f, moveX);
			info.hitWall = true;
		}
	}
}

void Enemy::OnGroundToggle(const CollisionMapInfo& info) {
	if (onGround_) {
		if (velocity_.y > 0.0f) {
			onGround_ = false;
		} else {
			MapChipType mapChipType;
			bool hit = false;
			float microValue = 0.01f;

			Vector3 leftBottomTarget = CornerPosition(worldTransform_.translation_, kLeftBottom);
			Vector3 rightBottomTarget = CornerPosition(worldTransform_.translation_, kRightBottom);
			leftBottomTarget.y -= microValue;
			rightBottomTarget.y -= microValue;

			MapChipField::IndexSet indexSet;
			indexSet = mapChipField_->GetMapChipIndexByPosition(leftBottomTarget);
			mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
			if (mapChipType == MapChipType::kBlock)
				hit = true;

			indexSet = mapChipField_->GetMapChipIndexByPosition(rightBottomTarget);
			mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
			if (mapChipType == MapChipType::kBlock)
				hit = true;

			if (!hit) {
				onGround_ = false;
			}
		}
	} else {
		if (info.landing) {
			onGround_ = true;
			velocity_.x *= (1.0f - kAttenuationLanding);
			velocity_.y = 0.0f;
		}
	}
}

void Enemy::ApplyMove(const CollisionMapInfo& info) {
	worldTransform_.translation_.x += info.move.x;
	worldTransform_.translation_.y += info.move.y;
	worldTransform_.translation_.z += info.move.z;
}

void Enemy::OnCeilingCollision(const CollisionMapInfo& info) {
	if (info.ceilingCollision) {
		velocity_.y = 0.0f;
	}
}

void Enemy::OnWallCollision(const CollisionMapInfo& info) {
	if (info.hitWall) {
		velocity_.x *= (1.0f - kAttenuationWall);
	}
}

// --- ワールド座標の取得 ---
Vector3 Enemy::GetWorldPosition() {
	Vector3 worldPos;
	// ワールド行列の平行移動成分を取得
	worldPos.x = worldTransform_.matWorld_.m[3][0];
	worldPos.y = worldTransform_.matWorld_.m[3][1];
	worldPos.z = worldTransform_.matWorld_.m[3][2];
	return worldPos;
}

// --- AABBの計算と取得 ---
AABB Enemy::GetAABB() {
	Vector3 worldPos = GetWorldPosition();
	AABB aabb;

	// エネミーの当たり判定サイズ（kWidth, kHeight）に基づいて計算
	aabb.min = {worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f};
	aabb.max = {worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f};

	return aabb;
}

// --- 衝突応答処理 ---
void Enemy::OnCollision(const Player* player) {
	(void)player;
}

void Enemy::Draw() { 
model_->Draw(worldTransform_, *camera_); 
}