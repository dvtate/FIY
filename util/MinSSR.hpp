//
// Created by tate on 3/10/26.
//
//
// Primitive template engine with optimization for constexpr keys
//

#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <algorithm>
#include <utility>
#include <array>
#include <cassert>
#include <type_traits>

namespace MinSSR {
    /**
     * Concatenate components into a single string, reserving space before appending them
     * @tparam Args char or StringViewLike
     * @param args parts to combine into single string
     * @return concatenated string
     */
    template<typename... Args>
    __attribute__((always_inline))
    constexpr std::string concat(const Args&... args) {
        constexpr auto to_sv = []<typename T>(const T& v) constexpr {
            if constexpr (std::is_same_v<T, char>)
                return std::string_view(&v, 1);
            else
                return std::string_view(v);
        };
        return [](const auto ...sv) constexpr {
            std::string ret;
            ret.reserve((sv.size() + ...));
            (ret.append(sv), ...);
            return ret;
        }(to_sv(args)...);
    }

    /**
     * Sorts tags array at compile-time
     * @tparam R should be std::array<std::string_view, N>
     * @param tags input, potentially unsorted array of tags
     * @return sorted array of tags
     */
    template<std::ranges::random_access_range R>
    consteval R sort_tags(R&& tags) {
        std::ranges::sort(tags);
        return tags;
    }

    /**
     * Use binary search to find the index of a tag in the sorted tags array
     * @tparam R type of tags object (should be std::array<std::string_view, N>)
     * @param sorted_tags sorted array of tags
     * @param tag tag to find in the array
     * @return index of tag in the array or array.size() if not found
     */
    template<std::ranges::random_access_range R>
    constexpr size_t index(
        const R& sorted_tags,
        const std::string_view tag
    ) {
#if __cplusplus >= 202302L
        if consteval {
            if (!std::is_sorted(sorted_tags.cbegin(), sorted_tags.cend()))
                throw "tags not sorted";
        }
#endif

        // Binary search
        auto it = std::ranges::lower_bound(sorted_tags, tag);

        // Not found
        if (it == sorted_tags.end() || *it != tag)
            return sorted_tags.size();

        // Return index
        return std::distance(sorted_tags.begin(), it);
    }

    // TODO constraints on R1 R2 sizes
    /**
     * Replace all {{tags}} in template_string with corresponding substitutions
     * @param template_string mustache template string
     * @param sorted_tags sorted array of tags to replace with corresponding substitutions
     * @param substitutions replacements to apply,
     *      if one larger than sorted_tags, the last value will replace unknown values
     * @return new string with replacements
     * @remark tags with index greater than
     */
    template<
        std::ranges::random_access_range R1,
        std::ranges::random_access_range R2>
    [[nodiscard]] constexpr std::string mustache(
        const std::string_view template_string,
        const R1& sorted_tags,
        const R2& substitutions
    ) {
        // index of {{ , key + value
        std::vector<std::pair<size_t, ssize_t>> replacements;
        replacements.reserve(sorted_tags.size()); // reasonable starting point

        // Calculate output length
        std::size_t len = template_string.size();
        std::size_t i = 0;
        while ((i = template_string.find("{{", i)) != std::string::npos) {
            const std::size_t start = i + 2;
            const std::size_t end = template_string.find("}}", start);
            if (end == std::string_view::npos) [[unlikely]] {
                break;
            }
            const auto tag = template_string.substr(start, end - start);
            const auto idx = index(sorted_tags, tag);
            if (idx < substitutions.size()) {
                replacements.emplace_back(i, idx == sorted_tags.size() ? -tag.size() - 1 : idx);
                len -= 4; // {{ }}
                len -= tag.size();
                len += substitutions[idx].size();
            }

            // put i after the }}
            i = end + 2;
        }

        // Create output
        std::string ret;
        ret.reserve(len);
        i = 0;
        for (const auto& p : replacements) {
            const std::size_t l = p.first - i;
            ret.append(template_string, i, l);
            if (p.second < 0)
                ret.append(substitutions[substitutions.size() - 1]);
            else
                ret.append(substitutions[p.second]);
            i += l;
            i += 4; // {{}}
            if (p.second < 0)
                i -= p.second + 1;
            else
                i += sorted_tags[p.second].size();
        }
        ret.append(template_string, i, -1);
        return ret;
    }

    /// When the template is constant we can remember where replacements need to occur
    class ParsedTemplate {
        /// Template string with template params removed
        std::string stripped_template;

        /// Locations + substitution index to replace in the template string
        /// when substitution index is negative it's an unknown tag and = -(orig length) - 1
        // TODO splitting this into 2 vectors could give better cache efficiency
        std::vector<std::pair<size_t, ssize_t>> substitution_points;

    public:
        /**
         * Parse a template using barebones mustache syntax
         * @param template_string input template
         * @param sorted_tags sorted array of tags used in the template
         * @param leave_unknown will unknown tags be ignored (true) or replaced (false)?
         */
        template<std::ranges::random_access_range R>
        constexpr ParsedTemplate(
            const std::string_view template_string,
            const R& sorted_tags,
            const bool leave_unknown = true
        ) {
            // TODO improved algorithm: copy+edit template string

            // Find all substitution points in template string
            size_t prev = 0;
            size_t i = 0;
            size_t index_offset = 0;
            while ((i = template_string.find("{{", prev)) != std::string::npos) {
                index_offset += i - prev;

                const size_t start = i + 2;
                const size_t end = template_string.find("}}", start);
                if (end == std::string_view::npos) [[unlikely]] {
                    index_offset += template_string.size() - i;
                    break;
                }
                const auto l = end - start;
                const auto tag = template_string.substr(start, l);
                const auto idx = index(sorted_tags, tag);

                if (idx == sorted_tags.size()) {
                    if (leave_unknown) {
                        // Not a substitution, include tag
                        index_offset += l + 4;
                    } else {
                        // Replace unknown
                        this->substitution_points.emplace_back(index_offset, -1 - (ssize_t)l); // default handler
                    }
                } else {
                    // Substitution
                    this->substitution_points.emplace_back(index_offset, idx);
                }

                // Put i after the }}
                prev = end + 2;
            }

            // Remove tags from template string to save memory
            // TODO resize_and_overwrite
            this->stripped_template.reserve(index_offset);
            i = 0;              // index in original template string
            index_offset = 0;   // index in stripped string
            for (auto& p : this->substitution_points) {
                // End of last -> start of current
                const std::size_t l = p.first - index_offset;

                // Copy non-param part of template
                this->stripped_template.append(template_string, i, l);

                // Don't copy param into stripped string
                index_offset += l;

                // Translated index
                i += l; // non-param part
                i += 4; // {{}}
                i += p.second < 0
                    ? -p.second - 1
                    : sorted_tags[p.second].size();
            }

            // Include end
            if (i < template_string.size())
                this->stripped_template.append(template_string, i);

            // Relinquish excess capacity
            this->substitution_points.shrink_to_fit();
        }

        /**
         * Populate the parsed template with the corresponding runtime values
         * @param substitutions list of corresponding replacements w/ default handler at the end
         * @return replaced string
         * @remark unknown value must be either the last element of the array or the template must not
         *  have any unknown values (ie - parsed with leave_unknown = true)
         */
        template<std::ranges::random_access_range R>
        constexpr std::string process(const R& substitutions) const {
            // Calculate size
            size_t len = stripped_template.size();
            for (const auto& p : substitution_points)
                if (p.second < 0)
                    len += substitutions[substitutions.size() - 1].size();
                else
                    len += substitutions[p.second].size();

            // Construct string
            std::string ret;
            ret.reserve(len);
            size_t prev = 0;
            for (const auto& p : substitution_points) {
                const auto l = p.first - prev;
                ret.append(stripped_template, prev, l);
                if (p.second < 0)
                    ret.append(substitutions[substitutions.size() - 1]);
                else
                    ret.append(substitutions[p.second]);
                prev = p.first;
            }
            ret.append(stripped_template, prev);
            return ret;
        }
    };

    /**
     * Escape HTML characters in a string
     * @param non_html String to escape HTML characters in
     * @return string with HTML characters escaped
     */
    inline std::string escape_html(const std::string& non_html) {
        // There's probably a better way to do this
        std::string ret;
        ret.reserve(non_html.size());
        for (const char c : non_html) {
            switch (c) {
                case '<':
                    ret += "&lt;";
                    break;
                case '&':
                    ret += "&amp;";
                    break;
                case '>':
                    ret += "&gt;";
                    break;
                case '\'':
                    ret += "&apos;";
                    break;
                case '\"':
                    ret += "&quot;";
                    break;
                default:
                    ret += c;
            }
        }
        return ret;
    }

}

/* usage:
#define USER_PAGE_RULES(KV) \
    KV("fiy_user", user) \
    KV("fiy_domain", fiy::host().domain) \
    KV("request_user", request_user == nullptr ? "" : request_user) \
    KV("mod_baseurl", fiy::host().base_uri) \
    KV("fiy_display_name", user) \
    KV("fiy_user_bio", "")

    return MIN_SSR_MUSTACHE(template_string, USER_PAGE_RULES);
 */

#define FIY_MIN_SSR_TAG_CSV(k, v) k,
#define FIY_MIN_SSR_SET_REPLACEMENT(k, v) replacements[MinSSR::index(tags, k)] = v;
#define FIY_MIN_SSR_TAGS_COUNT(k, v) + 1
#define FIY_MIN_SSR_IS_TEMP_STRING(v) (std::is_same_v<std::remove_cvref_t<decltype((v))>, std::string> \
    && !std::is_lvalue_reference_v<decltype((v))>)
#define FIY_MIN_SSR_HAS_TEMP_STRING(k, v) || FIY_MIN_SSR_IS_TEMP_STRING(v)

#define MIN_SSR_TAGS(rules) (MinSSR::sort_tags(std::array<std::string_view, \
    0 rules(FIY_MIN_SSR_TAGS_COUNT)>({rules(FIY_MIN_SSR_TAG_CSV)})))

#define MIN_SSR_REPLACEMENTS_TYPE(rules) std::conditional_t< \
        false rules(FIY_MIN_SSR_HAS_TEMP_STRING), \
        std::array<std::string, tags.size()>, \
        std::array<std::string_view, tags.size()>>

#define MIN_SSR_REPLACEMENTS_TYPE_WITH_DEFAULT(rules, unknown) std::conditional_t< \
        FIY_MIN_SSR_IS_TEMP_STRING(unknown) rules(FIY_MIN_SSR_HAS_TEMP_STRING), \
        std::array<std::string, tags.size() + 1>, \
        std::array<std::string_view, tags.size() + 1>>

/// Use this when the tags are constexpr and the template is constant
#define MIN_SSR_MUSTACHE(template_string, rules) \
    ([&](){ \
        static constexpr auto tags = MIN_SSR_TAGS(rules);\
        static const auto tp = MinSSR::ParsedTemplate(template_string, tags);\
        MIN_SSR_REPLACEMENTS_TYPE(rules) replacements; \
        rules(FIY_MIN_SSR_SET_REPLACEMENT) \
        return tp.process(replacements); \
    })()

/// Use this when the tags are constexpr and the template is constant and you want to replace unknown tags with a value
#define MIN_SSR_MUSTACHE_WITH_DEFAULT(template_string, rules, unknown_handler) \
    ([&](){ \
        static constexpr auto tags = MIN_SSR_TAGS(rules);\
        static const auto tp = MinSSR::ParsedTemplate(template_string, tags, false);\
        MIN_SSR_REPLACEMENTS_TYPE_WITH_DEFAULT(rules, unknown_handler) replacements; \
        rules(FIY_MIN_SSR_SET_REPLACEMENT); \
        replacements[tags.size()] = unknown_handler; \
        return tp.process(replacements); \
    })()

/// Use this when the the tags are constexpr but the template is not constant or it's a one-off rendering
#define MIN_SSR_MUSTACHE_VARIABLE_TEMPLATE(template_string, rules) \
    ([&](){ \
        static constexpr auto tags = MIN_SSR_TAGS(rules);\
        MIN_SSR_REPLACEMENTS_TYPE(rules) replacements; \
        rules(FIY_MIN_SSR_SET_REPLACEMENT) \
        return MinSSR::mustache( template_string, tags, replacements); \
    })()

/// Use this when the the tags are constexpr but the template is not constant and you want to replace unknown tags
#define MIN_SSR_MUSTACHE_VARIABLE_TEMPLATE_WITH_DEFAULT(template_string, rules, unknown_handler) \
    ([&](){ \
        static constexpr auto tags = MIN_SSR_TAGS(rules);\
        MIN_SSR_REPLACEMENTS_TYPE_WITH_DEFAULT(rules, unknown_handler) replacements; \
        rules(FIY_MIN_SSR_SET_REPLACEMENT); \
        replacements[tags.size()] = unknown_handler; \
        return MinSSR::mustache( template_string, tags, replacements); \
    })()
