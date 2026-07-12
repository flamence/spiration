/**
 * @file extension_tab.h
 * @brief 扩展管理标签页，显示已加载的扩展列表。
 * @author clk
 */

#pragma once

#include <ui/tab_bar.h>
#include <utils/animation.h>
#include <vector>
#include <string>

namespace spiration {

class extension;

/**
 * @brief 扩展管理标签页。
 */
class extension_tab : public tab {
public:
    extension_tab();

    void paint(std::shared_ptr<renderer> renderer) override;
    void handle_event(const event_type& type, void* data) override;
    void tick(float dt_ms) override;
    void on_activate() override;

private:
    struct ext_info {
        std::string name;
        std::string version;
        std::string description;
    };

    std::vector<ext_info> extensions_;
    std::vector<float> item_heights_;
    color_transition hover_bg_{color::transparent()};
    int hovered_index_ = -1;
    int last_hovered_index_ = -1;
    void collect_extensions();
};

} // namespace spiration
