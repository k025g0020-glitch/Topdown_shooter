#include "PlayerBullet.h"
#include "MapChipField.h"
#include "calc.h"
#include <cassert>

using namespace KamataEngine;

void PlayerBullet::Initialize(Model* model, Camera* camera, MapChipField* mapChipField, const Vector3& position, const Vector3& velocity) {
	assert(model);
	assert(mapChipField);

	model_ = model;
	camera_ = camera;
	mapChipField_ = mapChipField;
	velocity_ = velocity;

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
	worldTransform_.scale_ = {0.6f, 0.6f, 0.6f};
}

void PlayerBullet::Update() {
	// 移動
	worldTransform_.translation_ = calc::Add(worldTransform_.translation_, velocity_);

	// 壁ブロックに入ったら消える
	MapChipField::IndexSet index = mapChipField_->GetMapChipIndexByPosition(worldTransform_.translation_);
	if (mapChipField_->GetMapChipTypeByIndex(index.x, index.y) == MapChipType::kBlock) {
		isDead_ = true;
	}

	// 寿命が尽きたら消える
	if (--deathTimer_ <= 0) {
		isDead_ = true;
	}

	worldTransform_.matWorld_ = calc::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();
}

void PlayerBullet::Draw() { model_->Draw(worldTransform_, *camera_); }

Vector3 PlayerBullet::GetWorldPosition() const {
	return {worldTransform_.matWorld_.m[3][0], worldTransform_.matWorld_.m[3][1], worldTransform_.matWorld_.m[3][2]};
}

AABB PlayerBullet::GetAABB() const {
	Vector3 worldPos = GetWorldPosition();
	AABB aabb;
	aabb.min = {worldPos.x - kSize / 2.0f, worldPos.y - kSize / 2.0f, worldPos.z - kSize / 2.0f};
	aabb.max = {worldPos.x + kSize / 2.0f, worldPos.y + kSize / 2.0f, worldPos.z + kSize / 2.0f};
	return aabb;
}

void PlayerBullet::OnCollision() { isDead_ = true; }
