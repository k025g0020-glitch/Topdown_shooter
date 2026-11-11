#define NOMINMAX
#include "Fade.h"

using namespace KamataEngine;

void Fade::Initialize() {
	// スプライト生成
	sprite_ = Sprite::Create(0, {0, 0});
	sprite_->SetSize(Vector2(1280, 720));
	sprite_->SetColor(Vector4(0, 0, 0, 1));
}

void Fade::Start(Status status, float duration) {
	status_ = status;
	duration_ = duration;
	counter_ = 0.0f;
}

void Fade::Stop() { status_ = Status::None; }

bool Fade::IsFinished() const {
	switch (status_) {
	case Status::FadeIn:
	case Status::FadeOut:
		if (counter_ >= duration_) {
			return true;
		} else {
			return false;
		}
	}
	return true;
}

void Fade::Update() {
	// フェード状態による分岐
	switch (status_) {
	case Status::None:
		// 何もしない
		break;

	case Status::FadeIn:
		// 1フレーム分の秒数をカウントアップ
		counter_ += 1.0f / 60.0f;
		// フェード継続時間に達したら打ち止め（std::minを使用）
		counter_ = std::min(counter_, duration_);

		// アルファ値を1.0fから0.0fへ減少させる
		if (duration_ > 0.0f) {
			float alpha = 1.0f - (counter_ / duration_);
			sprite_->SetColor(Vector4(0, 0, 0, std::clamp(alpha, 0.0f, 1.0f)));
		}
		break;

	case Status::FadeOut:
		// 1フレーム分の秒数をカウントアップ
		counter_ += 1.0f / 60.0f;
		// フェード継続時間に達したら打ち止め（std::minを使用）
		counter_ = std::min(counter_, duration_);

		// アルファ値を0.0fから1.0fへ増加させる
		if (duration_ > 0.0f) {
			float alpha = counter_ / duration_;
			sprite_->SetColor(Vector4(0, 0, 0, std::clamp(alpha, 0.0f, 1.0f)));
		}
		break;
	}
}

void Fade::Draw() {
	// フェードなしの場合は描画しない
	if (status_ == Status::None) {
		return;
	}

	Sprite::PreDraw();
	if (sprite_) {
		sprite_->Draw();
	}
	Sprite::PostDraw();
}