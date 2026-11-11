#include "CameraController.h"
#include <algorithm>

using namespace KamataEngine;


void CameraController::Initialize(KamataEngine::Camera* camera) {
	camera_ = camera;
}

void CameraController::Reset() {
	const WorldTransform& targetWorldTransform = target_->GetWorldTransform();

	camera_->translation_.x = targetWorldTransform.translation_.x + targetOffset_.x;
	camera_->translation_.y = targetWorldTransform.translation_.y + targetOffset_.y;
	camera_->translation_.z = targetWorldTransform.translation_.z + targetOffset_.z;
}

void CameraController::Update() {
	const WorldTransform& targetWorldTransform = target_->GetWorldTransform();

	Vector3 targetVelocity = target_->GetVelocity();

	const float kFixedCameraY = 6.0f;

	targetPosition_.x = targetWorldTransform.translation_.x + targetOffset_.x + targetVelocity.x * kVelocityBias;
	targetPosition_.y = kFixedCameraY + targetOffset_.y;
	targetPosition_.z = targetWorldTransform.translation_.z + targetOffset_.z + targetVelocity.z * kVelocityBias;

	camera_->translation_ = calc::Lerp(camera_->translation_, targetPosition_, kInterpolationRate);

	camera_->translation_.x = std::max(camera_->translation_.x, targetWorldTransform.translation_.x + kMargin.left);
	camera_->translation_.x = std::min(camera_->translation_.x, targetWorldTransform.translation_.x + kMargin.right);
	//camera_->translation_.y = std::max(camera_->translation_.y, targetWorldTransform.translation_.y + kMargin.bottom);
	//camera_->translation_.y = std::min(camera_->translation_.y, targetWorldTransform.translation_.y + kMargin.top);

	camera_->translation_.x = std::clamp(camera_->translation_.x, movableArea_.left, movableArea_.right);
	camera_->translation_.y = std::clamp(camera_->translation_.y, movableArea_.bottom, movableArea_.top);

	camera_->UpdateMatrix();
}