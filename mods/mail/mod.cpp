//
// Created by tate on 8/6/24.
//

#include "../../modlib/fiymod.hpp"

#include <iostream>
#include <cstring>
#include <string>
#include <vector>
#include <set>

#include "MailBox.hpp"

static MailBox g_mailbox;

static std::vector<std::string> split_string(const std::string& str,
                                             const std::string& delimiter)
{
    std::vector<std::string> strings;

    std::string::size_type pos = 0;
    std::string::size_type prev = 0;
    while ((pos = str.find(delimiter, prev)) != std::string::npos)
    {
        strings.emplace_back(str.substr(prev, pos - prev));
        prev = pos + delimiter.size();
    }

    // To get the last substring (or only, if delimiter is not found)
    strings.emplace_back(str.substr(prev));

    return strings;
}

static void send_mail(const fiy::Request& req) {
    // Unauthenticated
    if (req.user == nullptr) {
        req.respond( 401, "Unauthenticated");
        return;
    }

    // Invalid body
    // It should be destination(s) \n subject \n mail body
    std::string body = req.body;
    auto i = body.find('\n');
    if (i == std::string::npos) {
        req.respond( 400, "Invalid body");
        return;
    }

    // Get destinations
    const std::string to_str = body.substr(0, i);
    auto destinations = split_string(to_str, ","); // TODO trim
    const auto old_i = i + 1;
    i = body.find('\n', old_i);
    if (i == std::string::npos) {
        req.respond( 400);
        return;
    }

    // Get subject
    std::string subject = body.substr(old_i, i - old_i);
    const std::string local_dom = std::string("@") + fiy::host().domain;
    for (auto& recip: destinations)
        if (recip.find('@') == std::string::npos)
            recip += local_dom;

    // Get mail body
    std::string content = body.substr(i);
    std::cout <<"new mail: " <<req.global_user_str() <<" : " <<destinations[0] <<" : " <<subject <<" : " <<content<<std::endl;
    g_mailbox.push(Mail(req.global_user_str(), destinations, subject, content));

    // Distribute to peers
    if (req.domain == nullptr) {
        // Get unique remote destinations
        std::set<std::string> doms;
        for (const auto& user : destinations) {
            const auto at_pos = user.find('@');
            if (at_pos != std::string::npos) {
                doms.emplace(user.substr(at_pos + 1));
            }
        }
        doms.erase(fiy::host().domain);

        // Send to destination servers
        for (const auto& dom : doms) {
            auto* req2 = new fiy::Request(req);
            req2->domain = dom.c_str();
            fiy::host().request_mod("mail", req2, [req2](const fiy::Response* res) {
                if (res == nullptr)
                    fiy::host().log_warning("Failed to send mail to " + std::string(req2->domain));
                else
                    fiy::host().log_debug("Sent mail to: " + std::string(req2->domain));
                delete req2;
            });

            std::cout <<"Sending to: " <<dom <<std::endl;
            fiy::host().request("mail", &req);
        }
    }
    req.respond();
}

static const std::string& header_links() {
    static const std::string head =
        "<header><div class=\"bar\">"
        "<a class=\"wordmark\" href=\"/\">FIY<span>.</span></a>"
        "<div class=\"bar-right\">"
        "<nav aria-label=\"Main navigation\">"
        "<a href='" + fiy::host().host_base_uri() + "/portal'>Portal</a>"
        " <a href='" + fiy::host().base_uri + "/inbox'>Inbox</a>"
        " <a href='" + fiy::host().base_uri + "/outbox'>Outbox</a>"
        " <a href='" + fiy::host().base_uri + "/compose'>Compose</a>"
        "</nav>"
        "<button class=\"icon-btn\" id=\"theme-toggle\" type=\"button\" aria-label=\"Switch theme\">\u263e</button>"
        "</div></div></header>";
    return head;
}

static std::string mail_page_head(const std::string& title) {
    return "<!DOCTYPE html><html lang=\"en\"><head>"
        "<meta charset=\"UTF-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
        "<title>" + title + " &mdash; FIY Mail</title>"
        "<link rel=\"preconnect\" href=\"https://fonts.googleapis.com\">"
        "<link rel=\"preconnect\" href=\"https://fonts.gstatic.com\" crossorigin>"
        "<link rel=\"stylesheet\" href=\"/portal/main.css?v=4\">"
        "<script src=\"/portal/theme.js?v=4\"></script>"
        "</head><body>"
        + header_links() + "<main>";
}

static std::string mail_page_foot() {
    return "</main><footer>FIY &mdash; Federate It Yourself.</footer></body></html>";
}


static void handle_request(const fiy_request_t* request) {
    const auto& req = *static_cast<const fiy::Request*>(request);

    std::cout <<"Mail: Path: "<<req.path <<std::endl;
    std::cout <<"Mail: User: "<<req.user_str() <<std::endl;

    // Everything here requires a login
    if (req.user == nullptr) {
        static const fiy::Response no_auth_resp{
            303,
            "Location: " + fiy::host().host_base_uri() + "/portal/login",
            fiy::Body()
        };
        req.respond( no_auth_resp);
        return;
    }

    if (strcmp(req.path, "/send") == 0) {
        send_mail(req);
        return;
    } else if (strcmp(req.path, "/") == 0) {
        static const std::string html = mail_page_head("Mail")
            + "<div class=\"page-head\"><h1>Welcome to Mail!</h1>"
            "<p>This demo mod is used to test federation protocol while providing basic communication functionality."
            " Messages are stored in server memory and thus may disappear without notice."
            "</p>"
            "<div class=\"cta-row\"><a class=\"btn btn-primary\" href='" + fiy::host().base_uri + "/compose'>Compose</a>"
            "<a class=\"btn btn-secondary\" href='" + fiy::host().base_uri + "/inbox'>Inbox</a></div></div>"
            + mail_page_foot();
        req.respond( 200, "Content-Type: text/html", fiy::Body(html));
        return;
    } else if (strcmp(req.path, "/inbox") == 0) {
        // Authenticated local user
        if (req.domain == nullptr && req.user != nullptr) {
            const auto inbox = mail_page_head("Inbox")
                + "<div class=\"page-head\"><h1>Inbox</h1></div>"
                + g_mailbox.get_inbox_str(req.global_user_str())
                + mail_page_foot();
            req.respond( 200, "Content-Type: text/html", fiy::Body(inbox));
            return;
        }

        // Unauthenticated
        req.respond( 401, "Unauthenticated");
        return;
    } else if (strcmp(req.path, "/outbox") == 0) {
        if (req.domain == nullptr && req.user != nullptr) {
            const auto outbox = mail_page_head("Outbox")
                + "<div class=\"page-head\"><h1>Outbox</h1></div>"
                + g_mailbox.get_outbox_str(req.global_user_str())
                + mail_page_foot();
            req.respond( 200, "Content-Type: text/html", fiy::Body(outbox));
            return;
        }

        // Unauthenticated
        req.respond( 401, "Unauthenticated");
        return;

    } else if (strcmp(req.path, "/compose") == 0) {
        std::string compose_html = mail_page_head("Compose")
           + "<div class=\"page-head\"><h1>Compose</h1></div>\n"
           "<form action='javascript:submit()'>\n"
           "<div class='field'><label for='inp-to'>To</label><input type='text' id='inp-to' placeholder='tate@dvtt.net,test@example.com,jerry' /></div>\n"
           "<div class='field'><label for='inp-subject'>Subject</label><input type='text' id='inp-subject' placeholder='Important message' /></div>\n"
           "<div class='field'><label for='inp-body'>Body</label><textarea id='inp-body' rows='8'>Write your email here</textarea></div>\n"
           "<button class='btn btn-primary' type='submit'>Send</button>\n"
           "</form>\n"
           "<script>\n"
           "async function submit() {\n"
           "    fetch('send', {\n"
           "            method: 'POST',\n"
           "            body: document.getElementById('inp-to').value\n"
           "                  + '\\n' + document.getElementById('inp-subject').value\n"
           "                  + '\\n' + document.getElementById('inp-body').value\n"
           "    }).then(v => window.location = 'outbox')\n"
           "    .catch(console.error);\n"
           "}\n"
           "</script>\n"
           + mail_page_foot();
        req.respond( 200, "Content-Type: text/html", compose_html);
        return;
    } else if (strncmp(req.path, "/view/", strlen("/view/")) == 0) {
        const size_t idx = strtoul(req.path + strlen("/view/"), nullptr, 10);
        // TODO verify that the user has access to the email
        Mail m;
        if (! g_mailbox.get(idx, m)) {
            req.respond( 404, "Not found");
            return;
        }
        const auto body_str = mail_page_head(m.m_subject)
            + "<div class=\"page-head\"><h1>" + m.m_subject + "</h1></div>"
            + m.long_view()
            + mail_page_foot();
        req.respond( 200, "Content-Type: text/html", fiy::Body(body_str));
        return;
    }

    std::string body = "Hello, @";
    if (req.user != nullptr)
        body += req.user;
    body += "@";
    if (req.domain != nullptr)
        body += req.domain;
    body += "! <br/>Path: ";
//    body += req.method;
    body += " ";
    if (req.path != nullptr)
        body += req.path;

    req.respond( 404, "Not found.");
}

FIY_EXPORT fiy::ModInfo* start(const fiy_host_info_t* host_info) {
    static fiy::ModInfo mod_info = {
        .on_request = handle_request,
        .delete_user = [](const char* user) {
            g_mailbox.delete_user_mail(user);
        },
        .id = "mail",
        .version = "0.0",
    };
    fiy::host() = *host_info;
    return &mod_info;
}
