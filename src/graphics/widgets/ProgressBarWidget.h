#pragma once

#include "graphics/core/Widget.h"
#include "graphics/theme/Theme.h"
#include <string>

namespace dc
{
namespace gfx
{

class ProgressBarWidget : public Widget
{
public:
    ProgressBarWidget();

    void paint (Canvas& canvas) override;

    /// Set progress value in range [0.0, 1.0]. Clamps to bounds.
    void setProgress (double progress);
    double getProgress() const { return progress_; }

    /// Optional status text shown left-aligned inside the bar
    /// (e.g., "Scanning Vital..."). Empty string shows only percentage.
    void setStatusText (const std::string& text);
    const std::string& getStatusText() const { return statusText_; }

private:
    double progress_ = 0.0;
    std::string statusText_;
};

} // namespace gfx
} // namespace dc
