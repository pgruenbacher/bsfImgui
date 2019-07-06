shader ImguiBasic
{
	raster 
	{
		cull = none;
		scissor = true;
	};

	blend
	{
		target	
		{
			enabled = true;
			color = { srcA, srcIA, add };
			writemask = RGB;
		};
	};	
	
	depth
	{
		read = false;
		write = false;
	};
	
	code
	{
		[internal]
		cbuffer GUIParams
		{
			float gInvViewportWidth;
			float gInvViewportHeight;
			float gViewportYFlip;
		}	

		void vsmain(
			in float2 inPos : POSITION,
			in float2 uv : TEXCOORD0,
			in float4 inColor : COLOR0,

			out float4 oPosition : SV_Position,
			out float2 oUv : TEXCOORD0,
			out float4 oColor : COLOR0)
		{
			// // float4 tfrmdPos = mul(gWorldTransform, float4(inPos.xy, 0, 1));
			float4 tfrmdPos = float4(inPos.xy, 0, 1);
			float tfrmdX = -1.0f + (tfrmdPos.x * gInvViewportWidth);
			float tfrmdY = (1.0f - (tfrmdPos.y * gInvViewportHeight)) * gViewportYFlip;
			// float tfrmdX = -1.0f + (tfrmdPos.x * 0.0015625);
			// float tfrmdY = (1.0f - (tfrmdPos.y * 0.00277778)) * 1.0;

			oPosition = float4(tfrmdX, tfrmdY, 0, 1);
			// oPosition = float4(inPos.xy, 0, 1);
			// oPosition = inPos;
			oUv = uv;
			oColor = inColor;
		}

		[alias(gMainTexture)]
		SamplerState gMainTexSamp;
		Texture2D gMainTexture;

		float4 fsmain(in float4 inPos : SV_Position, float2 uv : TEXCOORD0, float4 inColor : COLOR0) : SV_Target
		{
			float4 color = gMainTexture.Sample(gMainTexSamp, uv) * inColor;
			// float4 color = float4(1.0, 0.0, 0.0, 1.0);
			return color;
		}
	};
};