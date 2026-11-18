#pragma once
#include "KamataEngine.h"
#include <string>
#include <vector>

enum class MapChipType {
	kBlank,
	kBlock,
	kDoor, // ボス部屋の扉（開いている間は通れる。閉じると kBlock に変わる）
};

struct MapChipData {
	std::vector<std::vector<MapChipType>> data;
};

class MapChipField {
public:
	// XZ平面上の範囲矩形（bottom = 手前(-Z), top = 奥(+Z)）
	struct Rect {
		float left;   // 左端 (-X)
		float right;  // 右端 (+X)
		float bottom; // 手前 (-Z)
		float top;    // 奥 (+Z)
	};

	struct IndexSet {
		uint32_t x;
		uint32_t y;
	};

	// CSVに置かれた配置マーカーの種類
	enum class SpawnType {
		kChaserGroup,  // 3: 近接型の集団
		kShooterGroup, // 4: 遠距離型の集団
		kSwarmGroup,   // 5: 群れ型の集団
		kBoss,         // 6: ボス
		kPlayerStart,  // 7: スタート地点
	};
	struct SpawnPoint {
		SpawnType type;
		IndexSet index;
	};

	void Initialize();
	void Update();
	void Draw();
	// CSVを読み込む。行数・列数はCSVの大きさで決まる
	void LoadMapChipCsv(const std::string& filePath);

	MapChipType GetMapChipTypeByIndex(uint32_t xIndex, uint32_t yIndex) const;
	// マップチップの種類を書き換える（扉を閉じるときなど）
	void SetMapChipTypeByIndex(uint32_t xIndex, uint32_t yIndex, MapChipType type);

	// ボス部屋の床か（CSVの 6, 8 のマス）
	bool IsBossRoomByIndex(uint32_t xIndex, uint32_t yIndex) const;

	// CSVに置かれた配置マーカーの一覧
	const std::vector<SpawnPoint>& GetSpawnPoints() const { return spawnPoints_; }

	KamataEngine::Vector3 GetMapChipPositionByIndex(uint32_t xIndex, uint32_t yIndex) const;

	Rect GetRectByIndex(uint32_t xIndex, uint32_t yIndex) const;

	IndexSet GetMapChipIndexByPosition(const KamataEngine::Vector3& position) const;

	uint32_t GetNumBlockVirtical() const { return numBlockVertical_; }
	uint32_t GetNumBlockHorizontal() const { return numBlockHorizontal_; }

private:
	static inline const float kBlockWidth = 1.0f;
	static inline const float kBlockHeight = 1.0f;

	// 見下ろし型ステージ: CSVの行(縦) = Z方向、列(横) = X方向
	uint32_t numBlockVertical_ = 0;
	uint32_t numBlockHorizontal_ = 0;

	MapChipData mapChipData_;
	// ボス部屋の床かどうか [行][列]
	std::vector<std::vector<bool>> bossRoom_;
	std::vector<SpawnPoint> spawnPoints_;
};
