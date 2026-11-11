#include "CameraController.h"
#include <algorithm>
#include <cmath>

using namespace KamataEngine;

void CameraController::Initialize(KamataEngine::Camera* camera) {
	camera_ = camera;
	camera_->Initialize();
	// 高さと距離から自キャラを見下ろす X軸回転角を求める
	camera_->rotation_ = {std::atan2(targetOffset_.y, -targetOffset_.z), 0.0f, 0.0f};
}

void CameraController::Reset() {
	const WorldTransform& targetWorldTransform = target_->GetWorldTransform();

	camera_->translation_ = calc::Add(targetWorldTransform.translation_, targetOffset_);
	camera_->UpdateMatrix();
}

void CameraController::Update() {
	const WorldTransform& targetWorldTransform = target_->GetWorldTransform();

	// 追従目標 = 自キャラ位置 + オフセット + 速度による先読み（XZ平面）
	Vector3 targetVelocity = target_->GetVelocity();
	targetPosition_ = calc::Add(targetWorldTransform.translation_, targetOffset_);
	targetPosition_.x += targetVelocity.x * kVelocityBias;
	targetPosition_.z += targetVelocity.z * kVelocityBias;

	// 補間して滑らかに追従
	camera_->translation_ = calc::Lerp(camera_->translation_, targetPosition_, kInterpolationRate);

	// 自キャラが画面外に出ないようマージンで制限（X・Z）
	camera_->translation_.x =
	    std::clamp(camera_->translation_.x, targetWorldTransform.translation_.x + targetOffset_.x + kMargin.left, targetWorldTransform.translation_.x + targetOffset_.x + kMargin.right);
	camera_->translation_.z =
	    std::clamp(camera_->translation_.z, targetWorldTransform.translation_.z + targetOffset_.z + kMargin.bottom, targetWorldTransform.translation_.z + targetOffset_.z + kMargin.top);

	// 移動可能範囲で制限（X・Z）
	camera_->translation_.x = std::clamp(camera_->translation_.x, movableArea_.left, movableArea_.right);
	camera_->translation_.z = std::clamp(camera_->translation_.z, movableArea_.bottom, movableArea_.top);

	camera_->UpdateMatrix();
}
