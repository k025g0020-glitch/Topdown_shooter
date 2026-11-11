#define NOMINMAX
#include <algorithm>
#include <array>
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

	worldTransform_.rotation_.y = std::numbers::pi_v<float> / 2.0f;
	lrDirection_ = LRDirection::kRight;

	turnFirstRotationY_ = worldTransform_.rotation_.y;
	turnTimer_ = 0.0f;

	onGround_ = true;
}

void Player::Update() {

	Move();

	// 衝突情報を初期化
	CollisionMapInfo collisionMapInfo;
	// 移動量に速度の値をコピー
	collisionMapInfo.move = velocity_;

	// 衝突判定関数を呼び出す
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

	// 各方向の衝突・接触に応じた応答処理を行う
	OnCeilingCollision(collisionMapInfo);

	// 壁に接触している場合の処理
	OnWallCollision(collisionMapInfo);

	// 判定結果を反映して移動させる
	ApplyMove(collisionMapInfo);

	// 接地状態の切り替え処理
	OnGroundToggle(collisionMapInfo);

	worldTransform_.matWorld_ = calc::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();

	MapChipField::IndexSet playerIndex = mapChipField_->GetMapChipIndexByPosition(worldTransform_.translation_);
}

void Player::Move() {

	Input* input = Input::GetInstance();
	Vector3 acceleration = {};
	if (onGround_) {
		if (input->PushKey(DIK_RIGHT)) {
			// 左移動中の右入力
			if (velocity_.x < 0.0f) {
				// 速度と逆方向に入力中は急ブレーキ
				velocity_.x *= (1.0f - kAttenuation);
			}
			acceleration.x += kAcceleration;

			// 向きを右に更新
			if (lrDirection_ != LRDirection::kRight) {
				lrDirection_ = LRDirection::kRight;
				// 旋回開始時の角度を記録する
				turnFirstRotationY_ = worldTransform_.rotation_.y;
				// 旋回タイマーに時間を設定する
				turnTimer_ = kTimeTurn;
			}
		} else if (input->PushKey(DIK_LEFT)) {
			// 右移動中の左入力
			if (velocity_.x > 0.0f) {
				// 速度と逆方向に入力中は急ブレーキ
				velocity_.x *= (1.0f - kAttenuation);
			}
			acceleration.x -= kAcceleration;

			// 向きを左に更新
			if (lrDirection_ != LRDirection::kLeft) {
				lrDirection_ = LRDirection::kLeft;
				// 旋回開始時の角度を記録する
				turnFirstRotationY_ = worldTransform_.rotation_.y;
				// 旋回タイマーに時間を設定する
				turnTimer_ = kTimeTurn;
			}
		}

		if (input->PushKey(DIK_RIGHT) || input->PushKey(DIK_LEFT)) {
			velocity_.x += acceleration.x;
			velocity_.x = std::clamp(velocity_.x, -kLimitRunSpeed, kLimitRunSpeed);
		} else {
			velocity_.x *= (1.0f - kAttenuation);
		}

		if (input->TriggerKey(DIK_UP)) {
			velocity_.y += kJumpAcceleration;
		}

	} else {
		// 空中（落下速度の加算）
		velocity_.y -= kGravityAcceleration;
		// 落下速度制限（下方向への速度を一定値に丸める）
		velocity_.y = std::max(velocity_.y, -kLimitFallSpeed);
	}
}

Vector3 Player::CornerPosition(const Vector3& center, Corner corner) {
	Vector3 offsetTable[kNumCorner] = {
	    {+kWidth / 2.0f, -kHeight / 2.0f, 0.0f}, // kRightBottom
	    {-kWidth / 2.0f, -kHeight / 2.0f, 0.0f}, // kLeftBottom
	    {+kWidth / 2.0f, +kHeight / 2.0f, 0.0f}, // kRightTop
	    {-kWidth / 2.0f, +kHeight / 2.0f, 0.0f}, // kLeftTop
	};

	Vector3 result = {center.x + offsetTable[static_cast<uint32_t>(corner)].x, center.y + offsetTable[static_cast<uint32_t>(corner)].y, center.z + offsetTable[static_cast<uint32_t>(corner)].z};

	return result;
}

// マップ衝突判定の主処理
void Player::MapCollision(CollisionMapInfo& info) {
	MapCollisionUp(info);
	MapCollisionDown(info);
	MapCollisionRight(info);
	MapCollisionLeft(info);
}

// 各方向の衝突判定
void Player::MapCollisionUp(CollisionMapInfo& info) {
	if (info.move.y <= 0.0f) {
		return;
	}

	// 移動後の4つの角の座標をまとめて計算
	std::array<Vector3, kNumCorner> positionsNew;

	for (uint32_t i = 0; i < positionsNew.size(); ++i) {
		Vector3 targetPos = {worldTransform_.translation_.x + info.move.x, worldTransform_.translation_.y + info.move.y, worldTransform_.translation_.z + info.move.z};
		positionsNew[i] = CornerPosition(targetPos, static_cast<Corner>(i));
	}

	MapChipType mapChipType;
	MapChipType mapChipTypeNext;
	// 真上の当たり判定を行う
	bool hit = false;

	// 左上点の判定
	MapChipField::IndexSet indexSet;
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kLeftTop]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y + 1);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	// 右上点の判定
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

// 各方向の衝突判定（下方向）
void Player::MapCollisionDown(CollisionMapInfo& info) {
	if (info.move.y >= 0.0f) {
		return;
	}

	std::array<Vector3, kNumCorner> positionsNew;
	for (uint32_t i = 0; i < positionsNew.size(); ++i) {
		Vector3 targetPos = {worldTransform_.translation_.x + info.move.x, worldTransform_.translation_.y + info.move.y, worldTransform_.translation_.z + info.move.z};
		positionsNew[i] = CornerPosition(targetPos, static_cast<Corner>(i));
	}

	MapChipType mapChipType;
	MapChipType mapChipTypeNext;
	bool hit = false;

	// 左下点の判定
	MapChipField::IndexSet indexSet;
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kLeftBottom]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y - 1);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	// 右下点の判定
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

// 右方向の衝突判定
void Player::MapCollisionRight(CollisionMapInfo& info) {
	if (info.move.x <= 0.0f) {
		return;
	}

	// 移動後の4つの角の座標をまとめて計算
	std::array<Vector3, kNumCorner> positionsNew;
	for (uint32_t i = 0; i < positionsNew.size(); ++i) {
		Vector3 targetPos = {worldTransform_.translation_.x + info.move.x, worldTransform_.translation_.y + info.move.y, worldTransform_.translation_.z + info.move.z};
		positionsNew[i] = CornerPosition(targetPos, static_cast<Corner>(i));
	}

	MapChipType mapChipType;
	MapChipType mapChipTypeNext;
	bool hit = false;

	// 右上点の判定
	MapChipField::IndexSet indexSet;
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kRightTop]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.x - 1, indexSet.y);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	// 右下点の判定
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

// 左方向の衝突判定
void Player::MapCollisionLeft(CollisionMapInfo& info) {
	if (info.move.x >= 0.0f) {
		return;
	}

	std::array<Vector3, kNumCorner> positionsNew;
	for (uint32_t i = 0; i < positionsNew.size(); ++i) {
		Vector3 targetPos = {worldTransform_.translation_.x + info.move.x, worldTransform_.translation_.y + info.move.y, worldTransform_.translation_.z + info.move.z};
		positionsNew[i] = CornerPosition(targetPos, static_cast<Corner>(i));
	}

	MapChipType mapChipType;
	MapChipType mapChipTypeNext;
	bool hit = false;

	// 左上点の判定
	MapChipField::IndexSet indexSet;
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kLeftTop]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.x + 1, indexSet.y);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	// 左下点の判定
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

// 接地状態の切り替え処理
void Player::OnGroundToggle(const CollisionMapInfo& info) {
	// 自キャラが接地状態？
	if (onGround_) {
		// ジャンプ開始
		if (velocity_.y > 0.0f) {
			onGround_ = false;
		} else {
			// 落下判定（左右移動や床がなくなるなど、ジャンプせずに落下を始める場合の処理）
			MapChipType mapChipType;
			bool hit = false;

			// 足元より「微小な数値」だけ下方向の座標をサンプリングする
			float microValue = 0.01f;
			Vector3 leftBottomTarget = CornerPosition(worldTransform_.translation_, kLeftBottom);
			Vector3 rightBottomTarget = CornerPosition(worldTransform_.translation_, kRightBottom);

			leftBottomTarget.y -= microValue;
			rightBottomTarget.y -= microValue;

			MapChipField::IndexSet indexSet;

			// 左下点の判定
			indexSet = mapChipField_->GetMapChipIndexByPosition(leftBottomTarget);
			mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
			if (mapChipType == MapChipType::kBlock) {
				hit = true;
			}

			// 右下点の判定
			indexSet = mapChipField_->GetMapChipIndexByPosition(rightBottomTarget);
			mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.x, indexSet.y);
			if (mapChipType == MapChipType::kBlock) {
				hit = true;
			}

			// 落下開始（床が消失した）
			if (!hit) {
				// 空中状態に切り替え
				onGround_ = false;
			}
		}
	} else {
		// 空中にいる時（落下／ジャンプ）の処理
		// 着地フラグ
		if (info.landing) {
			// 接地状態に切り替える（落下を止める）
			onGround_ = true;
			// 着地時にX速度を減衰
			velocity_.x *= (1.0f - kAttenuationLanding);
			// Y速度をゼロにする
			velocity_.y = 0.0f;
		}
	}
}

// 判定結果を反映して移動させる
void Player::ApplyMove(const CollisionMapInfo& info) {
	// 移動
	worldTransform_.translation_.x += info.move.x;
	worldTransform_.translation_.y += info.move.y;
	worldTransform_.translation_.z += info.move.z;
}

// 天井に接触している場合の処理
void Player::OnCeilingCollision(const CollisionMapInfo& info) {
	if (info.ceilingCollision) {
		DebugText::GetInstance()->ConsolePrintf("hit ceiling\n");
		velocity_.y = 0.0f;
	}
}

// 壁に接触している場合の処理
void Player::OnWallCollision(const CollisionMapInfo& info) {
	// 壁接触による減衰
	if (info.hitWall) {
		velocity_.x *= (1.0f - kAttenuationWall);
	}
}

// --- ワールド座標の取得 ---
Vector3 Player::GetWorldPosition() {
	Vector3 worldPos;
	// ワールド行列の平行移動成分（4行目: m[3][0], m[3][1], m[3][2]）を取得
	worldPos.x = worldTransform_.matWorld_.m[3][0];
	worldPos.y = worldTransform_.matWorld_.m[3][1];
	worldPos.z = worldTransform_.matWorld_.m[3][2];
	return worldPos;
}

// --- AABBの計算と取得 ---
AABB Player::GetAABB() {
	Vector3 worldPos = GetWorldPosition();
	AABB aabb;

	// キャラクターの幅（kWidth）、高さ（kHeight）を半径分（/ 2.0f）としてボックスの最小・最大値を計算
	// 奥方向（Z）も同様の幅で判定を設定
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

void Player::Draw() { model_->Draw(worldTransform_, *camera_); }