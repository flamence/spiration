/**
 * @file i18n.h
 * @brief 国际化（i18n）字符串管理。
 * @author clk
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace spiration {

/**
 * @brief 国际化翻译管理器。
 */
class i18n_manager {
public:
    /**
     * @brief 从文件加载指定语言的翻译。
     * @param locale  语言代码
     * @param filepath 翻译文件路径
     * @return true 加载成功
     */
    static bool load(const std::string& locale, const std::string& filepath);

    /**
     * @brief 设置当前语言。
     */
    static void set_locale(const std::string& locale);

    /**
     * @brief 获取当前语言。
     */
    static const std::string& get_locale();

    /**
     * @brief 获取指定 key 的翻译文本。
     * @param key 翻译键
     * @param default_value 未找到时返回的默认值
     * @return 翻译后的字符串
     */
    static std::string tr(const std::string& key,
                          const std::string& default_value = "");

    /**
     * @brief 获取带参数替换的翻译文本。
     * @param key 翻译键
     * @param args 替换参数列表
     * @param default_value 未找到时返回的默认值
     * @return 参数替换后的字符串
     */
    static std::string tr(const std::string& key,
                          const std::vector<std::string>& args,
                          const std::string& default_value = "");

    /**
     * @brief 清除所有已加载的翻译。
     */
    static void clear();

    /**
     * @brief 获取所有已加载的语言列表。
     */
    static std::vector<std::string> available_locales();

private:
    struct locale_data {
        std::unordered_map<std::string, std::string> strings;
    };

    static std::unordered_map<std::string, locale_data> s_translations;
    static std::string s_current_locale;
};

} 
