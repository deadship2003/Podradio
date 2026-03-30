#include "podradio/common.h"
#include "podradio/logger.h"
#include "podradio/event_log.h"

namespace podradio
{

    // =========================================================
    // XML error handler implementations
    // (Must appear after LOG/EVENT_LOG macro definitions)
    // No longer outputs to terminal to avoid screen corruption
    // =========================================================

    void xml_error_handler(void* ctx, const char* msg, ...) {
        (void)ctx;  // libxml2 callback standard signature, ctx parameter unused
        char buffer[1024];
        va_list args;
        va_start(args, msg);
        vsnprintf(buffer, sizeof(buffer), msg, args);
        va_end(args);

        // Log to LOG file
        std::string err_msg = buffer;
        // Remove trailing newlines
        while (!err_msg.empty() && (err_msg.back() == '\n' || err_msg.back() == '\r')) {
            err_msg.pop_back();
        }

        if (!err_msg.empty()) {
            LOG(fmt::format("[XML] {}", err_msg));
            g_last_xml_error = err_msg;
            g_xml_error_count++;

            // Only show first error to EVENT_LOG (avoid flooding)
            if (g_xml_error_count == 1) {
                EVENT_LOG(fmt::format("XML Error: {}", err_msg.substr(0, 60)));
            }
        }
    }

    // xmlStructuredErrorFunc signature requires const xmlError* to match libxml2 API
    void xml_structured_error_handler(void* ctx, const xmlError* error) {
        (void)ctx;  // libxml2 callback standard signature, ctx parameter unused
        if (error && error->message) {
            std::string msg = error->message;
            // Remove trailing newlines
            while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r')) {
                msg.pop_back();
            }

            LOG(fmt::format("[XML] Line {}: {}", error->line, msg));
            g_last_xml_error = msg;
            g_xml_error_count++;

            if (g_xml_error_count == 1) {
                EVENT_LOG(fmt::format("XML Error (L{}): {}", error->line, msg.substr(0, 50)));
            }
        }
    }

    void reset_xml_error_state() {
        g_last_xml_error.clear();
        g_xml_error_count = 0;
    }

    std::string get_last_xml_error() {
        return g_last_xml_error;
    }

    // =========================================================
    // XML property retrieval helper function
    // =========================================================

    std::string get_xml_prop_any(xmlNodePtr n, std::vector<std::string> ns) {
        for (auto& name : ns) {
            xmlChar* p = xmlGetProp(n, (const xmlChar*)name.c_str());
            if (p) { std::string s = (char*)p; xmlFree(p); return s; }
        }
        return "";
    }

} // namespace podradio
