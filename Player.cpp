#define NOMINMAX
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "Enemy.h"
#include "MapChipField.h"
#include "Player.h"
#include "calc.h"

using namespace KamataEngine;

void Player::Initialize(Model* model, Camera* camera, MapChipField* mapChipField, const Vector3& position) {
	assert(model);
	assert(mapChipField);

	model_ = model;
	camera_ = camera;

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
	mapChipField_ = mapChipField;

	// 初期は奥(+Z)向き
	worldTransform_.rotation_.y = 0.0f;
	targetRotationY_ = worldTransform_.rotation_.y;
}

void Player::Update() {

	// 入力による移動量の計算
	Move();

	// 衝突情報を初期化
	CollisionMapInfo collisionMapInfo;
	// 移動量に速度の値をコピー
	collisionMapInfo.move = velocity_;

	// 衝突判定関数を呼び出す
	MapCollision(collisionMapInfo);

	// 壁に接触している場合の処理
	OnWallCollision(collisionMapInfo);

	// 判定結果を反映して移動させる
	ApplyMove(collisionMapInfo);

	// 旋回制御
	Turn();

	worldTransform_.matWorld_ = calc::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();

	// 攻撃
	Attack();

	// 弾の更新
	for (PlayerBullet* bullet : bullets_) {
		bullet->Update();
	}

	// 寿命が尽きた・壁や敵に当たった弾を削除
	bullets_.remove_if([](PlayerBullet* bullet) {
		if (bullet->IsDead()) {
			delete bullet;
			return true;
		}
		return false;
	});
}

void Player::Attack() {
	if (fireTimer_ > 0) {
		--fireTimer_;
	}

	// SPACE を押している間、一定間隔で連射
	if (!Input::GetInstance()->PushKey(DIK_SPACE) || fireTimer_ > 0 || !modelBullet_) {
		return;
	}
	fireTimer_ = kFireInterval;

	// 向いている方向（入力方向）に飛ばす。モデルの正面は +Z
	Vector3 velocity = {std::sin(targetRotationY_) * kBulletSpeed, 0.0f, std::cos(targetRotationY_) * kBulletSpeed};

	PlayerBullet* newBullet = new PlayerBullet();
	newBullet->Initialize(modelBullet_, camera_, mapChipField_, worldTransform_.translation_, velocity);
	bullets_.push_back(newBullet);
}

Player::~Player() {
	for (PlayerBullet* bullet : bullets_) {
		delete bullet;
	}
	bullets_.clear();
}

void Player::Move() {
	Input* input = Input::GetInstance();

	// 入力方向（XZ平面）
	Vector3 direction = {};
	if (input->PushKey(DIK_RIGHT) || input->PushKey(DIK_D)) {
		direction.x += 1.0f;
	}
	if (input->PushKey(DIK_LEFT) || input->PushKey(DIK_A)) {
		direction.x -= 1.0f;
	}
	if (input->PushKey(DIK_UP) || input->PushKey(DIK_W)) {
		direction.z += 1.0f;
	}
	if (input->PushKey(DIK_DOWN) || input->PushKey(DIK_S)) {
		direction.z -= 1.0f;
	}

	if (direction.x != 0.0f || direction.z != 0.0f) {
		// 斜め移動が速くならないよう正規化
		direction = calc::Normalize(direction);

		velocity_.x += direction.x * kAcceleration;
		velocity_.z += direction.z * kAcceleration;

		// 速度制限（XZ平面の速さで制限）
		float speed = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
		if (speed > kLimitRunSpeed) {
			velocity_.x *= kLimitRunSpeed / speed;
			velocity_.z *= kLimitRunSpeed / speed;
		}

		// 入力方向を向く（モデルの正面は +Z）
		targetRotationY_ = std::atan2(direction.x, direction.z);
	} else {
		// 入力がなければ減速
		velocity_.x *= (1.0f - kAttenuation);
		velocity_.z *= (1.0f - kAttenuation);
	}

	// 見下ろし型なので高さ方向には動かない
	velocity_.y = 0.0f;
}

void Player::Turn() {
	const float kPi = std::numbers::pi_v<float>;
	// 最短方向に回るよう角度差を -π ~ +π に収める
	float diff = std::remainder(targetRotationY_ - worldTransform_.rotation_.y, 2.0f * kPi);
	worldTransform_.rotation_.y += diff * kTurnRate;
}

Vector3 Player::CornerPosition(const Vector3& center, Corner corner) {
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
void Player::MapCollision(CollisionMapInfo& info) {
	MapCollisionFar(info);
	MapCollisionNear(info);
	MapCollisionRight(info);
	MapCollisionLeft(info);
}

// 奥(+Z)方向の当たり判定
void Player::MapCollisionFar(CollisionMapInfo& info) {
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
void Player::MapCollisionNear(CollisionMapInfo& info) {
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
void Player::MapCollisionRight(CollisionMapInfo& info) {
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
void Player::MapCollisionLeft(CollisionMapInfo& info) {
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
void Player::ApplyMove(const CollisionMapInfo& info) {
	worldTransform_.translation_.x += info.move.x;
	worldTransform_.translation_.z += info.move.z;
}

// 壁に接触している場合の処理
void Player::OnWallCollision(const CollisionMapInfo& info) {
	if (info.hitWallX) {
		velocity_.x *= (1.0f - kAttenuationWall);
	}
	if (info.hitWallZ) {
		velocity_.z *= (1.0f - kAttenuationWall);
	}
}

// --- ワールド座標の取得 ---
Vector3 Player::GetWorldPosition() {
	Vector3 worldPos;
	worldPos.x = worldTransform_.matWorld_.m[3][0];
	worldPos.y = worldTransform_.matWorld_.m[3][1];
	worldPos.z = worldTransform_.matWorld_.m[3][2];
	return worldPos;
}

// --- AABBの計算と取得 ---
AABB Player::GetAABB() {
	Vector3 worldPos = GetWorldPosition();
	AABB aabb;
	aabb.min = {worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f};
	aabb.max = {worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f};
	return aabb;
}

// --- 衝突応答処理 ---
void Player::OnCollision(const Enemy* enemy) {
	(void)enemy;

	// デスフラグを立てる
	isDead_ = true;
}

void Player::Draw() {
	model_->Draw(worldTransform_, *camera_);

	// 弾の描画
	for (PlayerBullet* bullet : bullets_) {
		bullet->Draw();
	}
}
