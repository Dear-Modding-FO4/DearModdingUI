// Separable Gaussian blur after Unrimp by Christian Ofenberg, MIT.

cbuffer BlurBuffer : register(b0)
{
	float4 TexelSize;  // xy = inverse size; w = downsample factor
	int4 BlurParams;   // x = samples
};

SamplerState LinearSampler : register(s0);
Texture2D InputTexture : register(t0);

struct VS_OUTPUT
{
	float4 Position: SV_POSITION;
	float2 TexCoord: TEXCOORD0;
};

static const float WEIGHTS[8] = {
	0.1760327f,
	0.1658591f,
	0.1403215f,
	0.1069852f,
	0.0732894f,
	0.0451904f,
	0.0248657f,
	0.0122423f
};

float4 GaussianBlur(VS_OUTPUT input, float2 direction)
{
	const int samples = min(BlurParams.x, 15);
	const int halfSamples = samples >> 1;

	float weightSum = WEIGHTS[0];
	[unroll(7)] for (int j = 1; j <= halfSamples; ++j)
	{
		weightSum += 2.0f * WEIGHTS[min(j, 7)];
	}
	const float normalization = 1.0f / weightSum;

	float4 result = InputTexture.Sample(LinearSampler, input.TexCoord) * (WEIGHTS[0] * normalization);

	[unroll(7)] for (int i = 1; i <= halfSamples; ++i)
	{
		float weight = WEIGHTS[min(i, 7)] * normalization;
		float2 offset = i * direction;

		result += InputTexture.Sample(LinearSampler, input.TexCoord + offset) * weight;
		result += InputTexture.Sample(LinearSampler, input.TexCoord - offset) * weight;
	}

	return result;
}

float4 PS_Horizontal(VS_OUTPUT input) : SV_TARGET
{
	return GaussianBlur(input, float2(TexelSize.x, 0.0f));
}

float4 PS_Vertical(VS_OUTPUT input) : SV_TARGET
{
	return GaussianBlur(input, float2(0.0f, TexelSize.y));
}
