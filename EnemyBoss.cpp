// ボスの行動（Enemy クラスのうち、ボス専用の部分）
#define NOMINMAX
#include <algorithm>
#include <cmath>
#include <numbers>

#include "Enemy.h"
#include "MapChipField.h"
#include "Player.h"
#include "calc.h"

using namespace KamataEngine;

void Enemy::StartBossBattle() {
	if (type_ != Type::kBoss || bossState_ != BossState::kSleep) {
		return;
	}
	isAggro_ = true;
	bossState_ = BossState::kWake;
	bossTimer_ = kBossWakeTime;
}

void Enemy::ThinkBoss() {
	Vector3 desiredVelocity = {};

	// 自キャラへの方向（XZ平面）
	Vector3 dirToPlayer = {};
	bool hasTarget = player_ && !player_->IsDead();
	if (hasTarget) {
		Vector3 toPlayer = calc::Subtract(player_->GetTranslation(), worldTransform_.translation_);
		toPlayer.y = 0.0f;
		if (calc::Length(toPlayer) > 0.001f) {
			dirToPlayer = calc::Normalize(toPlayer);
		}
	}
	const float kTwoPi = 2.0f * std::numbers::pi_v<float>;
	const float angleToPlayer = std::atan2(dirToPlayer.x, dirToPlayer.z);

	switch (bossState_) {
	case BossState::kSleep:
		// ボス戦開始まで何もしない
		break;

	case BossState::kWake:
		targetRotationY_ = angleToPlayer;
		if (--bossTimer_ <= 0) {
			EndBossAction();
		}
		break;

	case BossState::kChase:
		// ゆっくり追いかけ、時間が来たら次の攻撃へ
		desiredVelocity = calc::Multiply(param_.speed, dirToPlayer);
		targetRotationY_ = angleToPlayer;
		if (--bossTimer_ <= 0) {
			StartNextBossAction();
		}
		break;

	case BossState::kCharge:
		// 止まって自キャラの方を向きながら溜める
		targetRotationY_ = angleToPlayer;
		if (--bossTimer_ <= 0) {
			bossState_ = bossPendingAction_;
			bossVolleyCount_ = 0;
			bossTimer_ = 0;
			if (bossState_ == BossState::kDash) {
				// 溜め終わりの瞬間の自キャラの方向に突進する
				bossDashDirection_ = dirToPlayer;
				bossTimer_ = kBossDashTime;
				lastHitWall_ = false;
			}
		}
		break;

	case BossState::kRadial:
		// 全方位に弾をばらまく（撃つたびに角度を半分ずらして、すき間を変える）
		if (--bossTimer_ <= 0) {
			int32_t numBullets = IsBossPhase2() ? kBossRadialBulletsPhase2 : kBossRadialBullets;
			float step = kTwoPi / static_cast<float>(numBullets);
			float offset = (bossVolleyCount_ % 2 == 0) ? 0.0f : step / 2.0f;
			for (int32_t i = 0; i < numBullets; ++i) {
				float angle = offset + step * static_cast<float>(i);
				FireBullet({std::sin(angle), 0.0f, std::cos(angle)}, kBossBulletSpeed);
			}
			bossTimer_ = kBossRadialInterval;
			if (++bossVolleyCount_ >= kBossRadialVolleys) {
				EndBossAction();
			}
		}
		break;

	case BossState::kFan:
		// 自キャラに向けて扇状に弾を撃つ
		targetRotationY_ = angleToPlayer;
		if (--bossTimer_ <= 0) {
			for (int32_t i = 0; i < kBossFanBullets; ++i) {
				float t = static_cast<float>(i) / static_cast<float>(kBossFanBullets - 1); // 0 → 1
				float angle = angleToPlayer - kBossFanSpread + 2.0f * kBossFanSpread * t;
				FireBullet({std::sin(angle), 0.0f, std::cos(angle)}, kBossBulletSpeed * 1.3f);
			}
			bossTimer_ = kBossFanInterval;
			if (++bossVolleyCount_ >= kBossFanVolleys) {
				EndBossAction();
			}
		}
		break;

	case BossState::kDash:
		// 突進：速度を直接決める。壁に当たるか時間切れで止まり、すきができる
		velocity_ = calc::Multiply(kBossDashSpeed, bossDashDirection_);
		if (--bossTimer_ <= 0 || lastHitWall_) {
			bossState_ = BossState::kStun;
			bossTimer_ = kBossStunTime;
			velocity_ = {};
		}
		return;

	case BossState::kStun:
		if (--bossTimer_ <= 0) {
			EndBossAction();
		}
		break;

	case BossState::kSummon:
		// 周りに手下（群れ型）を呼び出す
		if (spawnCallback_) {
			for (int32_t i = 0; i < kBossSummonCount; ++i) {
				float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(kBossSummonCount);
				Vector3 position = worldTransform_.translation_;
				position.x += std::sin(angle) * kBossSummonRadius;
				position.z += std::cos(angle) * kBossSummonRadius;
				spawnCallback_(Type::kSwarm, position);
			}
		}
		EndBossAction();
		break;
	}

	// 目標速度へなめらかに近づける
	velocity_ = calc::Lerp(velocity_, desiredVelocity, kAccelRate);
	velocity_.y = 0.0f;
}

void Enemy::StartNextBossAction() {
	// 攻撃の順番（第2段階では手下の呼び出しが加わる）
	static const BossState kActionsPhase1[] = {BossState::kRadial, BossState::kFan, BossState::kDash};
	static const BossState kActionsPhase2[] = {BossState::kRadial, BossState::kSummon, BossState::kFan, BossState::kDash};

	if (IsBossPhase2()) {
		bossPendingAction_ = kActionsPhase2[bossActionIndex_ % std::size(kActionsPhase2)];
	} else {
		bossPendingAction_ = kActionsPhase1[bossActionIndex_ % std::size(kActionsPhase1)];
	}
	++bossActionIndex_;

	bossState_ = BossState::kCharge;
	bossChargeDuration_ = IsBossPhase2() ? kBossChargeTimePhase2 : kBossChargeTime;
	bossTimer_ = bossChargeDuration_;
}

void Enemy::EndBossAction() {
	bossState_ = BossState::kChase;
	bossTimer_ = IsBossPhase2() ? kBossChaseTimePhase2 : kBossChaseTime;
}
