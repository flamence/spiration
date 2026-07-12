/**
 * @file about_tab.h
 * @brief "关于"标签页，显示应用信息。
 * @author clk
 */

#pragma once

#include <ui/tab_bar.h>
#include <string>

namespace spiration {

/**
 * @brief "关于"标签页。
 */
class about_tab : public tab {
public:
    about_tab();

    void paint(std::shared_ptr<renderer> renderer) override;
    void handle_event(const event_type& type, void* data) override;

private:
    std::string app_name_;
    std::string app_version_;
    std::string platform_name_;
};

} // namespace spiration
