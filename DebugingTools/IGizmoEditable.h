#pragma once
#include <SimpleMath.h>

namespace HEIN
{
	class IGizmoEditable
	{
	public:

		~IGizmoEditable() = default;

		virtual void DrawGizmo(
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj,
			int opertaion,
			int mode
		) = 0;
	};
}