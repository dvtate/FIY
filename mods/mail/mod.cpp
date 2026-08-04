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
    static const std::string head = "<nav>"
        "<a href='" + fiy::host().host_base_uri() + "/portal'>" + fiy::host().domain + "</a>"
        " | <a href='" + fiy::host().base_uri + "/inbox'>Inbox</a>"
        " | <a href='" + fiy::host().base_uri + "/outbox'>Outbox</a>"
        " | <a href='" + fiy::host().base_uri + "/compose'>Compose</a>"
        "<hr/></nav>";
    return head;
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
        static const std::string html = header_links() + "<h1>Welcome to Mail!</h1>"
            "<p>This demo mod is used to test federation protocol while providing basic communication functionality."
            " Messages are stored in server memory and thus may disappear without notice."
            "</p>";
        req.respond( 200, "Content-Type: text/html", fiy::Body(html));
        return;
    } else if (strcmp(req.path, "/inbox") == 0) {
        // Authenticated local user
        if (req.domain == nullptr && req.user != nullptr) {
            const auto inbox = header_links() + g_mailbox.get_inbox_str(req.global_user_str());
            req.respond( 200, "Content-Type: text/html", fiy::Body(inbox));
            return;
        }

        // Unauthenticated
        req.respond( 401, "Unauthenticated");
        return;
    } else if (strcmp(req.path, "/outbox") == 0) {
        if (req.domain == nullptr && req.user != nullptr) {
            const auto outbox = header_links() + g_mailbox.get_outbox_str(req.global_user_str());
            req.respond( 200, "Content-Type: text/html", fiy::Body(outbox));
            return;
        }

        // Unauthenticated
        req.respond( 401, "Unauthenticated");
        return;

    } else if (strcmp(req.path, "/compose") == 0) {
        std::string compose_html =
            "<!doctype html><html><body>\n"
            + header_links() +
           "<form action='javascript:submit()'>\n"
           "To: <input type='text' id='inp-to' placeholder='tate@dvtt.net,test@example.com,jerry' />\n"
           "<br/>Subject: <input type='text' id='inp-subject' placeholder='Important message' />\n"
           "<br/>Body: <textarea id='inp-body'>Write your email here</textarea>\n"
           "<br/><button type='submit'>Send</button>\n"
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
           "</body></html>";
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
        const auto body_str = header_links() + m.long_view();
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
