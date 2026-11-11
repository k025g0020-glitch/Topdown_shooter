#pragma once
#include "KamataEngine.h"
#include "CollisionMapInfo.h"


using Vector3 = KamataEngine::Vector3;
using Matrix4x4 = KamataEngine::Matrix4x4;


class calc {
public:
	// 加算
	static Vector3 Add(const Vector3& v1, const Vector3& v2);
	// 減算
	static Vector3 Subtract(const Vector3& v1, const Vector3& v2);
	// スカラー倍
	static Vector3 Multiply(float scalar, const Vector3& v);
	// 内積
	static float Dot(const Vector3& v1, const Vector3& v2);
	// 長さ（ノルム）
	static float Length(const Vector3& v);
	// 正規化
	static Vector3 Normalize(const Vector3& v);

	// 数値 (float) の線形補間
	static float Lerp(float v1, float v2, float t);
	// ベクトル (Vector3) の線形補間
	static Vector3 Lerp(const Vector3& v1, const Vector3& v2, float t);

	// 行列の加法
	static Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2);
	// 行列の減法
	static Matrix4x4 Subtract(const Matrix4x4& m1, const Matrix4x4& m2);
	// 行列の積
	static Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);
	// 逆行列
	static Matrix4x4 Inverse(const Matrix4x4& m);
	// 転置行列
	static Matrix4x4 Transpose(const Matrix4x4& m);
	// 単位行列
	static Matrix4x4 MakeIdentity4x4();
	// 平行移動行列
	static Matrix4x4 MakeTranslateMatrix(const Vector3& translate);
	// 拡大縮小行列
	static Matrix4x4 MakeScaleMatrix(const Vector3& scale);
	// 座標交換
	static Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix);
	// X軸回転行列
	static Matrix4x4 MakeRotateXMatrix(float radian);
	// Y軸回転行列
	static Matrix4x4 MakeRotateYMatrix(float radian);
	// Z軸回転行列
	static Matrix4x4 MakeRotateZMatrix(float radian);
	// 3次元アフィン変換行列
	static Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);
	// AABB同士の交差判定関数
	static bool IsCollision(const AABB& aabb1, const AABB& aabb2);
};