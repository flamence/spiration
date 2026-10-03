#pragma once

#include <memory>
#include <functional>
#include <spiration/ui/appbar.h>
#include <spiration/ui/container.h>
#include <spiration/ui/context_menu.h>
#include <spiration/ui/menu_bar.h>
#include <spiration/ui/popup_menu.h>
#include <spiration/ui/tab_bar.h>
#include <spiration/window/window.h>

namespace spiration {

class label;

class root : public container {
private:
    std::shared_ptr<spiration::window> m_window;
    std::unique_ptr<popup_menu> m_popup = nullptr;
    std::unique_ptr<context_menu> m_context_menu = nullptr;
    /// @brief 延迟销毁的浮层，避免在控件自身事件处理中 delete-this。
    std::unique_ptr<popup_menu> m_popup_pending_ = nullptr;
    std::unique_ptr<context_menu> m_context_menu_pending_ = nullptr;

public:
    /**
     * @param parent 窗口对象。
     * @param create_appbar 是否创建 `appbar`。
     */
    explicit root(std::shared_ptr<spiration::window> parent, bool create_appbar = true);

    void init() override;
    void layout() override;
    void paint(std::shared_ptr<renderer> renderer) override;
    void tick(float dt_ms) override;
    void handle_event(const event_type& type, void* data) override;
    widget* hit_test_hover(float x, float y) const override;

    void show_popup(float x, float y, std::unique_ptr<popup_menu> popup);
    void dismiss_popup();
    bool has_popup() const { return m_popup != nullptr; }

    void show_context_menu(float x, float y, std::unique_ptr<context_menu> menu);
    void dismiss_context_menu();
    bool has_context_menu() const { return m_context_menu != nullptr; }

    /**
     * @brief 注册一个浮层控件。
     *        浮层在鼠标事件分发时优先接收；未消费则放行给普通内容。
     */
    void add_overlay(widget* w);
    /// @brief 注销浮层控件。
    void remove_overlay(widget* w);
    /// @brief 当前活跃浮层。
    widget* overlay() const { return m_overlay; }

    void set_mouse_capture(widget* w) override;

    /**
     * @brief 通知根控件。
     */
    void notify_selection_started(widget* w);

    /// @brief 开始一次跨控件文本选择会话。
    void begin_cross_selection(widget* w, size_t offset);
    /// @brief 拖拽过程中更新跨控件文本选择。
    void update_cross_selection(widget* from, float mx, float my);
    /// @brief 结束跨控件文本选择拖拽。
    void end_cross_selection(widget* w);

    /**
     * @brief 控件销毁时调用。
     */
    void on_widget_destroyed(widget* w);

    tab_bar* get_tab_bar() const { return tab_bar_; }
    menu_bar* get_menu_bar() const { return menu_bar_; }
    std::shared_ptr<spiration::window> window() const { return m_window; }

    /**
     * @brief 在指定菜单中添加子项。
     * @param menu_name 菜单标题或菜单键。
     * @param label     子项标签。
     * @param callback  点击回调。
     * @param owner     归属者标识（拓展 ID），空串表示宿主内置。
     */
    bool add_menu_item(const std::string& menu_name,
                       const std::string& label,
                       std::function<void()> callback,
                       const std::string& owner = std::string());

    /**
     * @brief 移除某归属者添加的全部菜单项。
     * @return 移除的数量。
     */
    size_t remove_menu_items(const std::string& owner);

    /**
     * @brief 在标签栏中打开一个新标签页。
     */
    void open_tab(std::unique_ptr<tab> t);

private:
    tab_bar* tab_bar_ = nullptr;
    menu_bar* menu_bar_ = nullptr;
    bool create_appbar_ = true;
    widget* captured_ = nullptr;
    widget* selecting_widget_ = nullptr;
    /// @brief 当前活跃浮层，事件优先路由给它。
    widget* m_overlay = nullptr;

    /// @brief 跨控件选择：当前鼠标所在的可选文本控件。
    widget* selection_cur_widget_ = nullptr;
    /// @brief 跨控件选择：起始控件内的字节偏移。
    size_t selection_anchor_ = 0;
    /// @brief 跨控件选择：当前控件内的字节偏移。
    size_t selection_pos_ = 0;
    /// @brief 是否正在拖拽选择。
    bool selection_dragging_ = false;

    /// @brief 沿父链向上查找最近的可选文本控件。
    label* nearest_selectable_label(widget* w) const;
    /// @brief 清除树中除 keep 外所有可选文本控件的选择。
    void clear_all_selections_except(widget* keep);
    /// @brief 根据锚点与当前位置刷新各控件选择区间。
    void apply_cross_selection();
};

} 
