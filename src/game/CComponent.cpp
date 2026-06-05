
#include "CComponent.h"

CComponent::CComponent(const COMP_TYPE type) noexcept :
    _iType(type)
{
}

CComponent::~CComponent() = default;

COMP_TYPE CComponent::GetType() const noexcept
{
    return _iType;
}

