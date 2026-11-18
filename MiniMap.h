#pragma once
#include "KamataEngine.h"
#include <vector>

class MapChipField;

// ミニマップ（画面右上）と全体マップ（TABで切り替え）
// 自キャラが通った周りだけが写る（PoE方式）
class MiniMap {
public:
	~MiniMap();
	void Initialize(const MapChipField* mapChipField);
	// playerPosition: 自キャラの位置（この周りを「探索済み」にする）
	void Update(const KamataEngine::Vector3& playerPosition);
	void Draw();

	// ボスの位置を設定（探索済みの場所にいれば印を出す）。倒したら hasBoss = false
	void SetBoss(bool hasBoss, const KamataEngine::Vector3& position = {});

	bool IsMapOpen() const { return isMapOpen_; }

private:
	// 探索済みの床をまとめた長方形（マス単位）
	struct CellRect {
		int32_t x;      // 左端の列
		int32_t y;      // 上端の行
		int32_t width;  // 横のマス数
		int32_t height; // 縦のマス数
	};

	// 自キャラの周りを探索済みにする
	void Reveal(int32_t centerX, int32_t centerY);
	// 探索済みの床を長方形にまとめ直す（探索範囲が広がったときだけ）
	void RebuildRects();
	// マップを描く範囲と縮尺を決めて、スプライトを並べる
	// clipLeft～clipBottom: 描いてよい画面上の範囲, cellSize: 1マスの大きさ<ピクセル>
	// originX/Y: 画面上の (clipLeft, clipTop) に来るマスの座標
	void LayoutMap(float clipLeft, float clipTop, float clipRight, float clipBottom, float cellSize, float originX, float originY);
	// プールからスプライトを1つ取り出して長方形を設定する（足りなければ false）
	bool PlaceSprite(float left, float top, float right, float bottom, const KamataEngine::Vector4& color);

	const MapChipField* mapChipField_ = nullptr;
	int32_t numCols_ = 0;
	int32_t numRows_ = 0;

	// 探索済みかどうか [行 * 列数 + 列]
	std::vector<bool> explored_;
	std::vector<CellRect> rects_;
	bool isDirty_ = true;
	int32_t lastCellX_ = -1;
	int32_t lastCellY_ = -1;

	// 自キャラの位置（マス単位・小数あり）
	float playerCellX_ = 0.0f;
	float playerCellY_ = 0.0f;

	// ボス
	bool hasBoss_ = false;
	float bossCellX_ = 0.0f;
	float bossCellY_ = 0.0f;

	bool isMapOpen_ = false;

	// スプライト
	KamataEngine::Sprite* spriteMiniBack_ = nullptr; // ミニマップの背景
	KamataEngine::Sprite* spriteMapBack_ = nullptr;  // 全体マップの背景
	std::vector<KamataEngine::Sprite*> pool_;        // 床・印に使い回すスプライト
	uint32_t usedCount_ = 0;

	// 探索する半径<マス>
	static inline const int32_t kRevealRadius = 10;
	// ミニマップ（画面右上）
	static inline const float kMiniSize = 220.0f;     // 一辺<ピクセル>
	static inline const float kMiniMargin = 20.0f;    // 画面の端からの距離
	static inline const float kMiniCellSize = 4.0f;   // 1マスの大きさ<ピクセル>
	// 全体マップの余白
	static inline const float kMapMarginX = 120.0f;
	static inline const float kMapMarginY = 60.0f;
	// スプライトの最大数
	static inline const uint32_t kPoolSize = 800;
};
