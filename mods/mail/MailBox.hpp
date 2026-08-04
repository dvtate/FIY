//
// Created by tate on 8/6/24.
//

#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <ctime>

#include "../../util/FileCache.hpp"

struct Mail {
    std::string m_from_user;
    std::vector<std::string> m_recipients;
    std::string m_subject;
    std::string m_body;
    time_t m_date{0};
    size_t m_index{0};

    Mail() = default;

    Mail(std::string from, std::vector<std::string> to, std::string subject, std::string body, time_t ts = std::time(nullptr)):
        m_from_user(std::move(from)),
        m_recipients(std::move(to)),
        m_subject(std::move(subject)),
        m_body(std::move(body)),
        m_date(ts)
    {}

    [[nodiscard]] std::string short_view() const {
        return concat(
            "<tr>",
            "<td><a href='view/",
            std::to_string(m_index),
            "'>",
            asctime(gmtime(&m_date)),
            "</a></td><td>",
            m_from_user,
            "</td><td>",
            m_subject.size() > 20
                ? m_subject.substr(0, 17) + "..."
                : m_subject,
            "</td></tr>"
        );
    }

    [[nodiscard]] std::string long_view() const {
        std::string ret;
        ret += "From: ";
        ret += m_from_user;
        ret += "<hr/>To: ";
        for (const auto& u : m_recipients)
            ret += u + ", ";
        ret.pop_back();
        ret.pop_back();
        ret += "<hr/>Subject: ";
        ret += m_subject;
        ret += "<hr/>";
        ret += m_body;
        ret += "<hr/>Received: ";
        ret += asctime(gmtime(&m_date));
        return ret;
    }
};

class MailBox {
    std::vector<Mail> m_mail; // deque better for reference stability
    std::mutex m_mtx;
public:
    std::vector<Mail> get_inbox(const std::string& local_user) {
        std::vector<Mail> ret;
        std::lock_guard lock{m_mtx};
        for (const auto& m : m_mail)
            for (const auto& u : m.m_recipients)
                if (u == local_user) {
                    ret.emplace_back(m);
                    break;
                }
        return ret;
    }

    std::string get_inbox_str(const std::string& local_user) {
        std::string ret = "<table><tr><th>Date</th><th>From</th><th>Subject</ht><tr/>";

        std::lock_guard lock{m_mtx};
        for (const auto& m : m_mail)
            for (const auto& u : m.m_recipients)
                if (u == local_user)
                    ret += m.short_view();
        ret += "</table>";
        return ret;
    }

    std::vector<Mail> get_outbox(const std::string& local_user) {
        std::vector<Mail> ret;
        std::lock_guard lock{m_mtx};
        for (const auto& m : m_mail)
            if (m.m_from_user == local_user)
                ret.emplace_back(m);
        return ret;
    }

    std::string get_outbox_str(const std::string& local_user) {
        std::string ret = "<table><tr><th>Date</th><th>From</th><th>Subject</ht><tr/>";
        std::lock_guard lock{m_mtx};
        for (const auto& m : m_mail)
            if (m.m_from_user == local_user)
                ret += m.short_view();
        ret += "</table>";
        return ret;
    }

    size_t push(Mail&& mail) {
        std::lock_guard lock{m_mtx};
        size_t ret = m_mail.size();
        mail.m_index = ret;
        m_mail.emplace_back(mail);
        return ret;
    }

    bool get(const size_t index, Mail& mail) {
        std::lock_guard lock{m_mtx};
        if (index >= m_mail.size())
            return false;
        mail = m_mail[index];
        return true;
    }

    void delete_user_mail(const char* user) {
        std::lock_guard lock{m_mtx};
        // Remove user from recipients list
        for (auto& m : m_mail)
            std::erase_if(
                m.m_recipients,
                [user](const auto& u){ return u == user; }
            );

        // Delete any mail that they sent or were the sole recipient of
        std::erase_if(m_mail, [user](const Mail& m) {
            return m.m_recipients.empty() || m.m_from_user == user;
        });
    }
};
