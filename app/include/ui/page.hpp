#pragma once

struct IPage
{
    virtual ~IPage() = default;
    virtual void Draw() = 0;
};