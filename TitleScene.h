#pragma once
#include "Fade.h"
#include "KamataEngine.h"

class TitleScene {
public:

	enum class Phase {
		kFadeIn,  // フェードイン
		kMain,    // メイン部
		kFadeOut, // フェードアウト
	};

	~TitleScene(); // デストラクタを追加

	void Initialize();
	void Update();
	void Draw();

	// 終了フラグのgetter
	bool IsFinished() const { return finished_; }

private:
	// フェード時間（秒）
	static inline const float kFadeDuration = 1.0f;

	// 現在のフェーズ
	Phase phase_ = Phase::kFadeIn;

	// 終了フラグ
	bool finished_ = false;

	// フェードオブジェクト
	Fade* fade_ = nullptr;
};