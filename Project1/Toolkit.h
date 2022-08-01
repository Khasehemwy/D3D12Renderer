#pragma once
#include <vector>

class Toolkit
{
public:
	static std::vector<float> CalcGaussWeights(float sigma);

private:
	Toolkit() = delete;
	~Toolkit() = delete;
};