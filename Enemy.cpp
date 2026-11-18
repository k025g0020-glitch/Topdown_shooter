#define NOMINMAX
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "Enemy.h"
#include "EnemyBullet.h"
#include "Player.h"
#include "MapChipField.h"
#include "calc.h"

using namespace KamataEngine;

Enemy::Param Enemy::GetParam(Type type) {
	switch (type) {
	case Type::kShooter:
		// 遠距離型：紫色。足は遅めで、距離をとって弾を撃つ
		return {30, 10, 0.07f, 11.0f, 1.0f, 0.8f, {0.8f, 0.4f, 1.0f, 1.0f}};
	case Type::kSwarm:
		// 群れ型：小さい緑色。HPは通常弾1発分、足が速い
		return {10, 8, 0.15f, 10.0f, 0.6f, 0.5f, {0.5f, 1.0f, 0.5f, 1.0f}};
	case Type::kBoss:
		// ボス：大きく赤い。HPが多く、接触ダメージも大きい
		return {1000, 30, 0.05f, 0.0f, 3.0f, 2.4f, {1.0f, 0.45f, 0.45f, 1.0f}};
	case Type::kChaser:
	default:
		// 近接型：通常色。気づいたら追いかけてくる
		return {30, 20, 0.11f, 9.0f, 1.0f, 0.8f, {1.0f, 1.0f, 1.0f, 1.0f}};
	}
}

void Enemy::Initialize(Model* model, Camera* camera, MapChipField* mapChipField, const Vector3& position, Type type) {
	assert(model);
	assert(mapChipField);

	model_ = model;
	camera_ = camera;
	mapChipField_ = mapChipField;

	type_ = type;
	param_ = GetParam(type);
	hp_ = param_.maxHp;
	width_ = param_.width;
	height_ = kHeight * param_.scale;

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
	worldTransform_.scale_ = {param_.scale, param_.scale, param_.scale};
	// 小さい敵が宙に浮かないよう、足元を床に合わせる（等倍のとき足元は y = -0.4）
	worldTransform_.translation_.y = -0.4f * (1.0f - param_.scale);

	// 手前(-Z)向き（カメラの方を向く）
	worldTransform_.rotation_.y = std::numbers::pi_v<float>;
	targetRotationY_ = worldTransform_.rotation_.y;

	// 歩行アニメーションがそろわないよう、開始タイミングをずらす
	walkTimer_ = position.x * 0.37f + position.z * 0.11f;

	hitTimer_ = 0;
	isAggro_ = false;
	// 撃つタイミングも敵ごとにずらす
	shootTimer_ = kShootInterval / 2 + static_cast<int32_t>(position.x * 7.0f) % kShootInterval;

	objectColor_.Initialize();
	objectColor_.SetColor(param_.color);
}

void Enemy::Update() {
	// 種類ごとの行動
	Think();

	// 衝突情報を初期化
	CollisionMapInfo collisionMapInfo;
	collisionMapInfo.move = velocity_;

	// マップとの衝突判定
	MapCollision(collisionMapInfo);
	lastHitWall_ = collisionMapInfo.hitWallX || collisionMapInfo.hitWallZ;
	++animFrame_;

	// 衝突応答
	OnWallCollision(collisionMapInfo);

	// 判定結果を反映して移動
	ApplyMove(collisionMapInfo);

	// 旋回（最短方向に回る）
	float diff = std::remainder(targetRotationY_ - worldTransform_.rotation_.y, 2.0f * std::numbers::pi_v<float>);
	worldTransform_.rotation_.y += diff * kTurnRate;

	// 歩行アニメーション（動いている間だけX軸周りに前後に揺らす）
	float speed = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
	if (speed > 0.01f) {
		walkTimer_ += 1.0f / 60.0f * (speed / 0.1f);
		float sinParam = std::sin(2.0f * std::numbers::pi_v<float> * walkTimer_);
		float ratio = (sinParam + 1.0f) / 2.0f;
		float degree = -kWalkMotionAngleStart + (kWalkMotionAngleStart + kWalkMotionAngleEnd) * ratio;
		worldTransform_.rotation_.x = degree * (std::numbers::pi_v<float> / 180.0f);
	} else {
		worldTransform_.rotation_.x *= 0.8f;
	}

	// 通常時の色（ハイライト中は黄色っぽく明るくする）
	Vector4 baseColor = isHighlighted_ ? Vector4{1.0f, 1.0f, 0.4f, 1.0f} : param_.color;

	// 攻撃の溜め中は白く点滅して膨らむ（攻撃してくる合図）
	float chargeScale = 1.0f;
	float chargeRatio = GetChargeRatio();
	if (chargeRatio >= 0.0f) {
		chargeScale = 1.0f + (type_ == Type::kBoss ? 0.15f : 0.25f) * chargeRatio;
		if ((animFrame_ / 3) % 2 == 0) {
			baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
		}
	}

	if (type_ == Type::kBoss) {
		// 目を覚ます演出：脈打つように大きさが変わる
		if (bossState_ == BossState::kWake) {
			chargeScale = 1.0f + 0.1f * std::sin(static_cast<float>(animFrame_) * 0.5f);
		}
		// 突進後のすき：暗くなって動きが止まる（攻撃のチャンス）
		if (bossState_ == BossState::kStun) {
			baseColor = {baseColor.x * 0.5f, baseColor.y * 0.5f, baseColor.z * 0.5f, 1.0f};
		}
	}

	// 被弾演出
	float hitScale = 1.0f;
	if (hitTimer_ > 0) {
		--hitTimer_;
		// 経過割合 1.0（被弾直後）→ 0.0（演出終了）
		float t = static_cast<float>(hitTimer_) / static_cast<float>(kHitFlashTime);
		// 一瞬膨らんで元の大きさに戻る
		float maxHitScale = (type_ == Type::kBoss) ? kBossHitScale : kHitScale;
		hitScale = 1.0f + (maxHitScale - 1.0f) * t;
		// 2フレームごとに赤と通常色を切り替えて点滅
		if ((hitTimer_ / 2) % 2 == 0) {
			baseColor = {1.0f, 0.2f, 0.2f, 1.0f};
		}
	}
	objectColor_.SetColor(baseColor);
	float scale = param_.scale * chargeScale * hitScale;
	worldTransform_.scale_ = {scale, scale, scale};

	// 行列更新
	worldTransform_.matWorld_ = calc::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();
}

void Enemy::Think() {
	// ボスは専用の行動
	if (type_ == Type::kBoss) {
		ThinkBoss();
		return;
	}

	Vector3 desiredVelocity = {};

	if (player_ && !player_->IsDead()) {
		// 自キャラへのベクトル（XZ平面）
		Vector3 toPlayer = calc::Subtract(player_->GetTranslation(), worldTransform_.translation_);
		toPlayer.y = 0.0f;
		float distance = calc::Length(toPlayer);
		Vector3 dirToPlayer = (distance > 0.001f) ? calc::Multiply(1.0f / distance, toPlayer) : Vector3{};

		// 一定距離まで近づいたら気づく（集団の仲間にも知らせる）
		if (!isAggro_ && distance <= param_.detectRange) {
			BecomeAggro();
		}

		if (isAggro_) {
			switch (type_) {
			case Type::kChaser:
			case Type::kSwarm:
				// まっすぐ追いかける
				desiredVelocity = calc::Multiply(param_.speed, dirToPlayer);
				break;

			case Type::kBoss:
				break;

			case Type::kShooter:
				// 近すぎたら離れ、遠すぎたら近づく。ちょうどいい距離ならその場で止まる
				if (distance < kShooterNearDistance) {
					desiredVelocity = calc::Multiply(-param_.speed, dirToPlayer);
				} else if (distance > kShooterFarDistance) {
					desiredVelocity = calc::Multiply(param_.speed, dirToPlayer);
				}
				UpdateShoot(distance);
				// 溜め中は止まる
				if (shootTimer_ <= kShootChargeTime) {
					desiredVelocity = {};
				}
				break;
			}

			// 自キャラの方を向く
			targetRotationY_ = std::atan2(dirToPlayer.x, dirToPlayer.z);
		}
	}

	// 仲間と重ならないように離れる
	desiredVelocity = calc::Add(desiredVelocity, CalcSeparation());

	// 目標速度へなめらかに近づける
	velocity_ = calc::Lerp(velocity_, desiredVelocity, kAccelRate);
	velocity_.y = 0.0f;
}

void Enemy::UpdateShoot(float distanceToPlayer) {
	if (--shootTimer_ > 0) {
		return;
	}
	shootTimer_ = kShootInterval;

	// 遠すぎる（画面外）ときは撃たない
	if (distanceToPlayer > param_.detectRange * 1.5f || !enemyBullets_ || !modelBullet_) {
		return;
	}

	// 撃つ瞬間の自キャラの位置へ向けて撃つ
	Vector3 toPlayer = calc::Subtract(player_->GetTranslation(), worldTransform_.translation_);
	toPlayer.y = 0.0f;
	FireBullet(calc::Normalize(toPlayer), kEnemyBulletSpeed);
}

void Enemy::FireBullet(const Vector3& direction, float speed) {
	if (!enemyBullets_ || !modelBullet_) {
		return;
	}
	// 弾は自キャラと同じ高さ(y = 0)を飛ばす
	Vector3 position = worldTransform_.translation_;
	position.y = 0.0f;

	EnemyBullet* bullet = new EnemyBullet();
	bullet->Initialize(modelBullet_, camera_, mapChipField_, position, calc::Multiply(speed, direction));
	enemyBullets_->push_back(bullet);
}

float Enemy::GetChargeRatio() const {
	if (type_ == Type::kShooter && isAggro_ && shootTimer_ <= kShootChargeTime) {
		return 1.0f - static_cast<float>(shootTimer_) / static_cast<float>(kShootChargeTime);
	}
	if (type_ == Type::kBoss && bossState_ == BossState::kCharge) {
		return 1.0f - static_cast<float>(bossTimer_) / static_cast<float>(bossChargeDuration_);
	}
	return -1.0f;
}

void Enemy::BecomeAggro() {
	isAggro_ = true;

	// 集団に属していなければ自分だけ
	if (groupId_ < 0 || !enemies_) {
		return;
	}

	// 同じ集団の仲間も一斉に気づく
	for (Enemy* other : *enemies_) {
		if (other != this && other->groupId_ == groupId_) {
			other->isAggro_ = true;
		}
	}
}

Vector3 Enemy::CalcSeparation() const {
	Vector3 push = {};
	// ボスは押されない
	if (!enemies_ || type_ == Type::kBoss) {
		return push;
	}
	for (const Enemy* other : *enemies_) {
		if (other == this || other->IsDead()) {
			continue;
		}
		Vector3 away = calc::Subtract(worldTransform_.translation_, other->worldTransform_.translation_);
		away.y = 0.0f;
		float distance = calc::Length(away);
		// 近いほど強く押し離す
		float radius = (width_ + other->width_) / 2.0f + 0.1f;
		radius = std::max(radius, kSeparationRadius * std::min(param_.scale, other->param_.scale));
		if (distance > 0.0001f && distance < radius) {
			push = calc::Add(push, calc::Multiply(kSeparationPower * (1.0f - distance / radius) / distance, away));
		}
	}
	return push;
}

Vector3 Enemy::CornerPosition(const Vector3& center, Corner corner) {
	Vector3 offsetTable[kNumCorner] = {
	    {+width_ / 2.0f, 0.0f, -width_ / 2.0f}, // kRightNear
	    {-width_ / 2.0f, 0.0f, -width_ / 2.0f}, // kLeftNear
	    {+width_ / 2.0f, 0.0f, +width_ / 2.0f}, // kRightFar
	    {-width_ / 2.0f, 0.0f, +width_ / 2.0f}, // kLeftFar
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
		MapChipField::IndexSet indexSetNow = mapChipField_->GetMapChipIndexByPosition({pos.x, 0.0f, pos.z + width_ / 2.0f});
		indexSet = mapChipField_->GetMapChipIndexByPosition({pos.x, 0.0f, pos.z + info.move.z + width_ / 2.0f});

		if (indexSetNow.y != indexSet.y) {
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.x, indexSet.y);
			float moveZ = rect.bottom - pos.z - (width_ / 2.0f + kBlank);
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
		MapChipField::IndexSet indexSetNow = mapChipField_->GetMapChipIndexByPosition({pos.x, 0.0f, pos.z - width_ / 2.0f});
		indexSet = mapChipField_->GetMapChipIndexByPosition({pos.x, 0.0f, pos.z + info.move.z - width_ / 2.0f});

		if (indexSetNow.y != indexSet.y) {
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.x, indexSet.y);
			float moveZ = rect.top + (width_ / 2.0f + kBlank) - pos.z;
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
		MapChipField::IndexSet indexSetNow = mapChipField_->GetMapChipIndexByPosition({pos.x + width_ / 2.0f, 0.0f, pos.z});
		indexSet = mapChipField_->GetMapChipIndexByPosition({pos.x + info.move.x + width_ / 2.0f, 0.0f, pos.z + info.move.z});

		if (indexSetNow.x != indexSet.x) {
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.x, indexSet.y);
			float moveX = rect.left - (width_ / 2.0f) - kBlank - pos.x;
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
		MapChipField::IndexSet indexSetNow = mapChipField_->GetMapChipIndexByPosition({pos.x - width_ / 2.0f, 0.0f, pos.z});
		indexSet = mapChipField_->GetMapChipIndexByPosition({pos.x + info.move.x - width_ / 2.0f, 0.0f, pos.z + info.move.z});

		if (indexSetNow.x != indexSet.x) {
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.x, indexSet.y);
			float moveX = rect.right + (width_ / 2.0f) + kBlank - pos.x;
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
Vector3 Enemy::GetWorldPosition() const {
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
	aabb.min = {worldPos.x - width_ / 2.0f, worldPos.y - height_ / 2.0f, worldPos.z - width_ / 2.0f};
	aabb.max = {worldPos.x + width_ / 2.0f, worldPos.y + height_ / 2.0f, worldPos.z + width_ / 2.0f};
	return aabb;
}

// --- 衝突応答処理 ---
void Enemy::OnCollision(const Player* player) {
	(void)player;
}

void Enemy::TakeDamage(int32_t damage) {
	if (isDead_) {
		return;
	}
	// 眠っているボスはダメージを受けない（部屋の外から倒されないように）
	if (type_ == Type::kBoss && bossState_ == BossState::kSleep) {
		return;
	}
	// 攻撃されたら、遠くにいても自キャラに気づく（集団の仲間にも知らせる）
	if (!isAggro_) {
		BecomeAggro();
	}
	// HPを減らす
	hp_ -= damage;
	// 被弾演出を開始
	hitTimer_ = kHitFlashTime;

	// HPが0になったら倒される
	if (hp_ <= 0) {
		isDead_ = true;
	}
}

void Enemy::Draw() {
	model_->Draw(worldTransform_, *camera_, &objectColor_);
}
