#define NOMINMAX
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <numbers>

#include "Enemy.h"
#include "FireField.h"
#include "FireParticles.h"
#include "Fireball.h"
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

	hp_ = kMaxHp;
	invincibleTimer_ = 0;
	isDead_ = false;

	mp_ = kMaxMp;
	skillCooldownTimers_ = {};
}

void Player::InitializeSkills(Model* fireModel) {
	modelFire_ = fireModel;
	fireParticles_ = new FireParticles();
	fireParticles_->Initialize(fireModel, camera_, mapChipField_);
}

void Player::Update() {

	// 入力による移動量の計算
	Move();

	// SPACE で前転開始
	StartRoll();

	// 衝突情報を初期化
	CollisionMapInfo collisionMapInfo;
	// 移動量（前転中は前転の速度で動く）
	collisionMapInfo.move = isRolling_ ? calc::Multiply(kRollSpeed, rollDirection_) : velocity_;

	// 衝突判定関数を呼び出す
	MapCollision(collisionMapInfo);

	// 壁に接触している場合の処理
	OnWallCollision(collisionMapInfo);

	// 判定結果を反映して移動させる
	ApplyMove(collisionMapInfo);
	moveAmount_ = collisionMapInfo.move;
	moveAmount_.y = 0.0f;

	// 無敵時間を減らす
	if (invincibleTimer_ > 0) {
		--invincibleTimer_;
	}
	// 前転のクールダウンを減らす
	if (rollCooldownTimer_ > 0) {
		--rollCooldownTimer_;
	}

	if (isRolling_) {
		// 前転中は回転演出のみ（向きは転がる方向で固定）
		UpdateRoll();
	} else {
		// 狙う敵の更新（敵の上で左クリック）
		UpdateTarget();

		// 狙う方向の決定（向きも狙う方向になる）
		Aim();

		// 旋回制御
		Turn();
	}

	worldTransform_.matWorld_ = calc::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();

	// スキル（前転中は使えない）
	if (!isRolling_) {
		UpdateSkills();
	} else {
		isChanneling_ = false;
	}

	// 通常攻撃（前転中・火炎放射中は撃てない）
	if (!isRolling_ && !isChanneling_) {
		Attack();
	} else if (fireTimer_ > 0) {
		--fireTimer_;
	}

	// 火炎球の更新と削除
	for (Fireball* fireball : fireballs_) {
		fireball->Update();
	}
	fireballs_.remove_if([](Fireball* fireball) {
		if (fireball->IsDead()) {
			delete fireball;
			return true;
		}
		return false;
	});

	// 炎の円の更新と削除
	for (FireField* field : fireFields_) {
		field->Update();
	}
	fireFields_.remove_if([](FireField* field) {
		if (field->IsDead()) {
			delete field;
			return true;
		}
		return false;
	});

	// 炎の見た目の更新
	if (fireParticles_) {
		fireParticles_->Update();
	}

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

	// 左クリックを押している間、一定間隔で連射（SPACE は回避用に空けておく）
	if (!Input::GetInstance()->IsPressMouse(0) || fireTimer_ > 0 || !modelBullet_) {
		return;
	}
	fireTimer_ = kFireInterval;

	// マウスカーソルの方向に飛ばす
	Vector3 velocity = calc::Multiply(kBulletSpeed, aimDirection_);

	PlayerBullet* newBullet = new PlayerBullet();
	newBullet->Initialize(modelBullet_, camera_, mapChipField_, worldTransform_.translation_, velocity);
	bullets_.push_back(newBullet);
}

void Player::UpdateSkills() {
	Input* input = Input::GetInstance();

	// クールダウンを減らす
	for (int32_t& timer : skillCooldownTimers_) {
		if (timer > 0) {
			--timer;
		}
	}

	if (!fireParticles_ || !enemies_) {
		return;
	}

	// Q: 押している間、火炎放射
	isChanneling_ = input->PushKey(DIK_Q) && HasEnoughMp(kSkillFlamethrower);
	if (isChanneling_) {
		mp_ -= kSkillMpCost[kSkillFlamethrower];
		UpdateFlamethrower();
	} else {
		// 火炎放射していないときだけMPが回復
		mp_ = std::min(kMaxMp, mp_ + kMpRegen);
	}

	// E: 火炎球をカーソル（または指定した敵）の方向へ発射
	if (input->TriggerKey(DIK_E) && skillCooldownTimers_[kSkillFireball] <= 0 && HasEnoughMp(kSkillFireball)) {
		mp_ -= kSkillMpCost[kSkillFireball];
		skillCooldownTimers_[kSkillFireball] = kSkillCooldown[kSkillFireball];

		Fireball* fireball = new Fireball();
		Vector3 spawnPosition = calc::Add(worldTransform_.translation_, calc::Multiply(0.8f, aimDirection_));
		fireball->Initialize(modelFire_, camera_, mapChipField_, enemies_, fireParticles_, spawnPosition, aimDirection_);
		fireballs_.push_back(fireball);
	}

	// R: 自キャラを中心に、地面に炎の円を出す
	if (input->TriggerKey(DIK_R) && skillCooldownTimers_[kSkillFireRing] <= 0 && HasEnoughMp(kSkillFireRing)) {
		mp_ -= kSkillMpCost[kSkillFireRing];
		skillCooldownTimers_[kSkillFireRing] = kSkillCooldown[kSkillFireRing];

		FireField* field = new FireField();
		field->Initialize(enemies_, fireParticles_, worldTransform_.translation_);
		fireFields_.push_back(field);
	}
}

void Player::UpdateFlamethrower() {
	static std::mt19937 engine{std::random_device{}()};
	std::uniform_real_distribution<float> spreadDist(-1.0f, 1.0f);
	std::uniform_real_distribution<float> speedDist(0.28f, 0.36f);

	const float kHalfAngle = kFlameHalfAngleDeg * std::numbers::pi_v<float> / 180.0f;
	const float baseAngle = std::atan2(aimDirection_.x, aimDirection_.z);

	// 見た目：口元から扇形に炎を噴き出す
	Vector3 origin = calc::Add(worldTransform_.translation_, calc::Multiply(0.6f, aimDirection_));
	for (int i = 0; i < 4; ++i) {
		float angle = baseAngle + spreadDist(engine) * kHalfAngle * 0.8f;
		float speed = speedDist(engine);
		Vector3 velocity = {std::sin(angle) * speed, 0.0f, std::cos(angle) * speed};
		fireParticles_->Emit(origin, velocity, 18, 0.3f, 1.2f);
	}

	// 一定間隔で、扇形の範囲内にいる敵にダメージ（自キャラは対象外）
	if (--flameTickTimer_ > 0) {
		return;
	}
	flameTickTimer_ = kFlameTickInterval;

	const float cosHalfAngle = std::cos(kHalfAngle);
	for (Enemy* enemy : *enemies_) {
		if (enemy->IsDead()) {
			continue;
		}
		Vector3 toEnemy = calc::Subtract(enemy->GetWorldPosition(), worldTransform_.translation_);
		toEnemy.y = 0.0f;
		float distance = calc::Length(toEnemy);
		if (distance > kFlameRange) {
			continue;
		}
		// 密着している敵は角度に関係なく当てる。それ以外は狙う方向との角度で判定
		if (distance < 0.5f || calc::Dot(calc::Normalize(toEnemy), aimDirection_) >= cosHalfAngle) {
			enemy->TakeDamage(kFlameDamage);
		}
	}
}

float Player::GetSkillCooldownRatio(SkillId skill) const {
	if (kSkillCooldown[skill] <= 0) {
		return 0.0f;
	}
	return static_cast<float>(skillCooldownTimers_[skill]) / static_cast<float>(kSkillCooldown[skill]);
}

bool Player::HasEnoughMp(SkillId skill) const { return mp_ >= kSkillMpCost[skill]; }

void Player::StartRoll() {
	if (isRolling_ || rollCooldownTimer_ > 0) {
		return;
	}
	if (!Input::GetInstance()->TriggerKey(DIK_SPACE)) {
		return;
	}

	// 移動入力があればその方向、なければマウスカーソルの方向へ転がる
	if (moveDirection_.x != 0.0f || moveDirection_.z != 0.0f) {
		rollDirection_ = moveDirection_;
	} else {
		rollDirection_ = aimDirection_;
	}

	isRolling_ = true;
	rollTimer_ = kRollTime;

	// 前転が終わったら止まった状態から動き出す
	velocity_ = {};

	// 転がる方向を即座に向く（モデルの正面は +Z）
	worldTransform_.rotation_.y = std::atan2(rollDirection_.x, rollDirection_.z);
	targetRotationY_ = worldTransform_.rotation_.y;
}

void Player::UpdateRoll() {
	--rollTimer_;

	// 経過割合 0.0 → 1.0
	float t = 1.0f - static_cast<float>(rollTimer_) / static_cast<float>(kRollTime);

	// X軸回りに1回転させて前転に見せる（+方向で前に倒れる）
	worldTransform_.rotation_.x = 2.0f * std::numbers::pi_v<float> * t;

	// 前転終了
	if (rollTimer_ <= 0) {
		isRolling_ = false;
		worldTransform_.rotation_.x = 0.0f;
		rollCooldownTimer_ = kRollCooldown;
	}
}

void Player::UpdateTarget() {
	Input* input = Input::GetInstance();

	// 狙っていた敵が倒されてリストから消えていたら解除
	if (targetEnemy_ && (!enemies_ || std::find(enemies_->begin(), enemies_->end(), targetEnemy_) == enemies_->end() || targetEnemy_->IsDead())) {
		targetEnemy_ = nullptr;
	}

	// カーソルの下の敵（ハイライト表示用）
	hoveredEnemy_ = FindEnemyUnderCursor();

	// 左クリックした瞬間：敵の上ならその敵を指定、地面なら指定なし
	if (input->IsTriggerMouse(0)) {
		targetEnemy_ = hoveredEnemy_;
	}

	// 左クリックを離したら指定を解除
	if (!input->IsPressMouse(0)) {
		targetEnemy_ = nullptr;
	}
}

void Player::Aim() {
	// 狙う位置：敵を指定していればその敵、いなければマウスカーソルの位置
	Vector3 aimPosition;
	if (targetEnemy_) {
		aimPosition = targetEnemy_->GetWorldPosition();
	} else if (!GetMouseWorldPosition(aimPosition)) {
		return;
	}

	// 自キャラから狙う位置へのベクトル（XZ平面）
	Vector3 toAim = calc::Subtract(aimPosition, worldTransform_.translation_);
	toAim.y = 0.0f;

	// 狙う位置が自キャラにほぼ重なっているときは方向を更新しない
	if (calc::Length(toAim) < 0.01f) {
		return;
	}
	aimDirection_ = calc::Normalize(toAim);

	// カーソル方向を向く（モデルの正面は +Z）
	targetRotationY_ = std::atan2(aimDirection_.x, aimDirection_.z);
}

void Player::GetMouseRay(Vector3& outOrigin, Vector3& outDirection) const {
	// マウス座標（クライアント座標, ピクセル）
	const Vector2& mousePosition = Input::GetInstance()->GetMousePosition();

	// スクリーン座標 → 正規化デバイス座標（-1～+1, Yは上が+）
	float ndcX = mousePosition.x / static_cast<float>(WinApp::kWindowWidth) * 2.0f - 1.0f;
	float ndcY = 1.0f - mousePosition.y / static_cast<float>(WinApp::kWindowHeight) * 2.0f;

	// ビュープロジェクション行列の逆行列で、NDC → ワールド座標に戻す
	Matrix4x4 matViewProjection = calc::Multiply(camera_->matView, camera_->matProjection);
	Matrix4x4 matInverseVP = calc::Inverse(matViewProjection);

	// 近クリップ面(z=0)と遠クリップ面(z=1)上の点を求め、カーソルを通るレイを作る
	Vector3 posNear = calc::Transform({ndcX, ndcY, 0.0f}, matInverseVP);
	Vector3 posFar = calc::Transform({ndcX, ndcY, 1.0f}, matInverseVP);

	outOrigin = posNear;
	outDirection = calc::Subtract(posFar, posNear);
}

bool Player::GetMouseWorldPosition(Vector3& outPosition) const {
	Vector3 posNear;
	Vector3 rayDirection;
	GetMouseRay(posNear, rayDirection);

	// レイが地面と平行なら交点なし
	if (std::abs(rayDirection.y) < 0.0001f) {
		return false;
	}

	// レイと平面 y = 自キャラの高さ の交点を求める
	float t = (worldTransform_.translation_.y - posNear.y) / rayDirection.y;
	if (t < 0.0f) {
		return false;
	}
	outPosition = calc::Add(posNear, calc::Multiply(t, rayDirection));
	return true;
}

Enemy* Player::FindEnemyUnderCursor() const {
	if (!enemies_) {
		return nullptr;
	}

	Vector3 origin;
	Vector3 direction;
	GetMouseRay(origin, direction);

	Enemy* nearestEnemy = nullptr;
	float nearestT = std::numeric_limits<float>::max();

	for (Enemy* enemy : *enemies_) {
		if (enemy->IsDead()) {
			continue;
		}

		// 敵のAABBを少し広げる
		AABB aabb = enemy->GetAABB();
		aabb.min = calc::Subtract(aabb.min, {kPickPadding, kPickPadding, kPickPadding});
		aabb.max = calc::Add(aabb.max, {kPickPadding, kPickPadding, kPickPadding});

		// レイとAABBの交差判定（スラブ法）
		// 各軸で「レイが箱の範囲に入っている区間 [tMin, tMax]」を求め、3軸すべてで重なれば当たり
		float tMin = 0.0f;
		float tMax = std::numeric_limits<float>::max();
		const float origins[3] = {origin.x, origin.y, origin.z};
		const float directions[3] = {direction.x, direction.y, direction.z};
		const float mins[3] = {aabb.min.x, aabb.min.y, aabb.min.z};
		const float maxs[3] = {aabb.max.x, aabb.max.y, aabb.max.z};
		bool hit = true;

		for (int axis = 0; axis < 3; ++axis) {
			if (std::abs(directions[axis]) < 0.00001f) {
				// この軸と平行：始点が範囲外なら当たらない
				if (origins[axis] < mins[axis] || origins[axis] > maxs[axis]) {
					hit = false;
					break;
				}
				continue;
			}
			float t1 = (mins[axis] - origins[axis]) / directions[axis];
			float t2 = (maxs[axis] - origins[axis]) / directions[axis];
			if (t1 > t2) {
				std::swap(t1, t2);
			}
			tMin = std::max(tMin, t1);
			tMax = std::min(tMax, t2);
			if (tMin > tMax) {
				hit = false;
				break;
			}
		}

		// 一番手前（カメラに近い）敵を選ぶ
		if (hit && tMin < nearestT) {
			nearestT = tMin;
			nearestEnemy = enemy;
		}
	}

	return nearestEnemy;
}

Player::~Player() {
	for (PlayerBullet* bullet : bullets_) {
		delete bullet;
	}
	bullets_.clear();
	for (Fireball* fireball : fireballs_) {
		delete fireball;
	}
	fireballs_.clear();
	for (FireField* field : fireFields_) {
		delete field;
	}
	fireFields_.clear();
	delete fireParticles_;
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

	moveDirection_ = {};
	if (direction.x != 0.0f || direction.z != 0.0f) {
		// 斜め移動が速くならないよう正規化
		direction = calc::Normalize(direction);
		moveDirection_ = direction;

		velocity_.x += direction.x * kAcceleration;
		velocity_.z += direction.z * kAcceleration;

		// 速度制限（XZ平面の速さで制限）
		float speed = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
		// 火炎放射中は遅くなる
		float limitSpeed = isChanneling_ ? kLimitRunSpeed * kChannelMoveSpeedRate : kLimitRunSpeed;
		if (speed > limitSpeed) {
			velocity_.x *= limitSpeed / speed;
			velocity_.z *= limitSpeed / speed;
		}

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
	// 敵に触れたら接触ダメージを受ける
	TakeDamage(enemy->GetContactDamage());
}

void Player::TakeDamage(int32_t damage) {
	// 無敵中・前転中・死亡後はダメージを受けない
	if (IsInvincible() || isRolling_ || isDead_) {
		return;
	}

	hp_ -= damage;
	invincibleTimer_ = kInvincibleTime;

	// HPが0になったら死亡
	if (hp_ <= 0) {
		hp_ = 0;
		isDead_ = true;
	}
}

void Player::Draw() {
	// 無敵中は4フレームごとに表示・非表示を切り替えて点滅
	bool isVisible = !IsInvincible() || (invincibleTimer_ / 4) % 2 == 0;
	if (isVisible) {
		model_->Draw(worldTransform_, *camera_);
	}

	// 弾の描画
	for (PlayerBullet* bullet : bullets_) {
		bullet->Draw();
	}

	// 火炎球の描画
	for (Fireball* fireball : fireballs_) {
		fireball->Draw();
	}

	// 炎の描画
	if (fireParticles_) {
		fireParticles_->Draw();
	}
}
