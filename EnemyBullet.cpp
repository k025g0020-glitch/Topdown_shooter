#include "EnemyBullet.h"
#include "MapChipField.h"
#include "calc.h"
#include <cmath>

using namespace KamataEngine;

void EnemyBullet::Initialize(Model* model, Camera* camera, MapChipField* mapChipField, const Vector3& position, const Vector3& velocity) {
	model_ = model;
	camera_ = camera;
	mapChipField_ = mapChipField;
	velocity_ = velocity;

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
	worldTransform_.scale_ = {kScale, kScale, kScale};

	// 自分の弾と見分けやすい紫色
	objectColor_.Initialize();
	objectColor_.SetColor({0.8f, 0.3f, 1.0f, 1.0f});
}

void EnemyBullet::Update() {
	// 移動
	worldTransform_.translation_ = calc::Add(worldTransform_.translation_, velocity_);

	// 回転させ、少し脈打つように大きさを変えて目立たせる
	++frameCount_;
	worldTransform_.rotation_ = calc::Add(worldTransform_.rotation_, {0.1f, 0.15f, 0.0f});
	float scale = kScale * (1.0f + 0.15f * std::sin(static_cast<float>(frameCount_) * 0.4f));
	worldTransform_.scale_ = {scale, scale, scale};

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

void EnemyBullet::Draw() { model_->Draw(worldTransform_, *camera_, &objectColor_); }

AABB EnemyBullet::GetAABB() const {
	const Vector3& pos = worldTransform_.translation_;
	AABB aabb;
	aabb.min = {pos.x - kSize / 2.0f, pos.y - kSize / 2.0f, pos.z - kSize / 2.0f};
	aabb.max = {pos.x + kSize / 2.0f, pos.y + kSize / 2.0f, pos.z + kSize / 2.0f};
	return aabb;
}
