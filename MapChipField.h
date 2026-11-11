#pragma once
#include "KamataEngine.h"


enum class MapChipType {
	kBlank,
	kBlock,
};

struct MapChipData {
	std::vector<std::vector<MapChipType>> data;
};

class MapChipField {
public:

	// 範囲矩形構造体を追加
	// XZ平面上の範囲矩形（bottom = 手前(-Z), top = 奥(+Z)）
	struct Rect {
		float left;   // 左端 (-X)
		float right;  // 右端 (+X)
		float bottom; // 手前 (-Z)
		float top;    // 奥 (+Z)
	};

	void Initialize();
	void Update();
	void Draw();
	void ResetMapChipData();
	void LoadMapChipCsv(const std::string& filePath);

	MapChipType GetMapChipTypeByIndex(uint32_t xIndex, uint32_t yIndex);

	KamataEngine::Vector3 GetMapChipPositionByIndex(uint32_t xIndex, uint32_t yIndex);

	Rect GetRectByIndex(uint32_t xIndex, uint32_t yIndex);

	struct IndexSet {
		uint32_t x;
		uint32_t y;
	};

	IndexSet GetMapChipIndexByPosition(const KamataEngine::Vector3& position);

	uint32_t GetNumBlockVirtical() const {
		return kNumBlockVirtical;
	}
	uint32_t GetNumBlockHorizontal() const { 
		return kNumBlockHorizontal; 
	}


private:

	static inline const float kBlockWidth = 1.0f;
	static inline const float kBlockHeight = 1.0f;

	// 見下ろし型ステージ: CSVの行(縦) = Z方向、列(横) = X方向
	static inline const uint32_t kNumBlockVirtical = 30;
	static inline const uint32_t kNumBlockHorizontal = 30;

	MapChipData mapChipData_;

};