#pragma once

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <ctime>
#include <utility>
#include <vector>

namespace dbd_server {

struct SpoolCommandFile {
    std::filesystem::path source_path {};
    std::filesystem::path claimed_path {};
    std::string command_id {};
    std::string receipt_path {};
    std::string result_path {};
    std::string payload {};
};

struct SpoolDirectories {
    std::filesystem::path root {};
    std::filesystem::path inbox {};
    std::filesystem::path processing {};
    std::filesystem::path receipts {};
    std::filesystem::path results {};
    std::filesystem::path archive {};
};

inline std::string CommandSpoolJsonEscape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    escaped += "\\u";
                    escaped += "00";
                    constexpr char kHex[] = "0123456789abcdef";
                    escaped.push_back(kHex[(static_cast<unsigned char>(ch) >> 4) & 0x0f]);
                    escaped.push_back(kHex[static_cast<unsigned char>(ch) & 0x0f]);
                } else {
                    escaped.push_back(ch);
                }
                break;
        }
    }
    return escaped;
}

inline std::optional<std::string> ExtractQuotedString(std::string_view object_text, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    const auto key_pos = object_text.find(needle);
    if (key_pos == std::string_view::npos) {
        return std::nullopt;
    }
    const auto colon = object_text.find(':', key_pos + needle.size());
    if (colon == std::string_view::npos) {
        return std::nullopt;
    }
    const auto first_quote = object_text.find('"', colon + 1);
    if (first_quote == std::string_view::npos) {
        return std::nullopt;
    }
    std::string value;
    bool escaping = false;
    for (std::size_t i = first_quote + 1; i < object_text.size(); ++i) {
        const char ch = object_text[i];
        if (escaping) {
            value.push_back(ch);
            escaping = false;
            continue;
        }
        if (ch == '\\') {
            escaping = true;
            continue;
        }
        if (ch == '"') {
            return value;
        }
        value.push_back(ch);
    }
    return std::nullopt;
}

inline std::string CommandSpoolTimestampUtc() {
    const auto now = std::chrono::system_clock::now();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    std::tm utc_time {};
#if defined(_WIN32)
    gmtime_s(&utc_time, &now_time);
#else
    gmtime_r(&now_time, &utc_time);
#endif

    std::ostringstream out;
    out << std::put_time(&utc_time, "%Y-%m-%dT%H:%M:%S")
        << '.'
        << std::setw(3) << std::setfill('0') << millis.count()
        << 'Z';
    return out.str();
}

inline std::string CommandSpoolFileTimestampUtc() {
    std::string timestamp = CommandSpoolTimestampUtc();
    for (char& ch : timestamp) {
        if (ch == ':' || ch == '.') {
            ch = '-';
        }
    }
    return timestamp;
}

inline std::string SanitizeCommandId(std::string value) {
    if (value.empty()) {
        return {};
    }
    for (char& ch : value) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (!(std::isalnum(uch) || ch == '-' || ch == '_' || ch == '.')) {
            ch = '_';
        }
    }
    while (!value.empty() && (value.front() == '.' || value.front() == ' ')) {
        value.erase(value.begin());
    }
    while (!value.empty() && (value.back() == '.' || value.back() == ' ')) {
        value.pop_back();
    }
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (lowered == "con" || lowered == "prn" || lowered == "aux" || lowered == "nul" ||
        lowered == "com1" || lowered == "com2" || lowered == "com3" || lowered == "com4" ||
        lowered == "com5" || lowered == "com6" || lowered == "com7" || lowered == "com8" ||
        lowered == "com9" || lowered == "lpt1" || lowered == "lpt2" || lowered == "lpt3" ||
        lowered == "lpt4" || lowered == "lpt5" || lowered == "lpt6" || lowered == "lpt7" ||
        lowered == "lpt8" || lowered == "lpt9") {
        value += "_";
    }
    return value;
}

inline std::filesystem::path NextAvailableSpoolPath(
    const std::filesystem::path& directory,
    const std::string& prefix,
    const std::string& extension) {
    std::filesystem::path candidate = directory / (prefix + extension);
    for (int suffix = 1; suffix < 10000 && std::filesystem::exists(candidate); ++suffix) {
        candidate = directory / (prefix + "-" + std::to_string(suffix) + extension);
    }
    return candidate;
}

inline std::optional<std::string> ReadTextFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return std::nullopt;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

inline bool WriteTextFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::trunc | std::ios::binary);
    if (!out.is_open()) {
        return false;
    }
    out << text;
    return out.good();
}

inline SpoolDirectories PrepareCommandSpool(const std::filesystem::path& runtime_root) {
    SpoolDirectories directories;
    directories.root = runtime_root / "command_spool";
    directories.inbox = directories.root / "inbox";
    directories.processing = directories.root / "processing";
    directories.receipts = directories.root / "receipts";
    directories.results = directories.root / "results";
    directories.archive = directories.root / "archive";

    std::filesystem::create_directories(directories.inbox);
    std::filesystem::create_directories(directories.processing);
    std::filesystem::create_directories(directories.receipts);
    std::filesystem::create_directories(directories.results);
    std::filesystem::create_directories(directories.archive);
    return directories;
}

inline void WriteCommandReceipt(
    const SpoolCommandFile& command_file,
    const std::string& status,
    const std::string& message,
    std::uint64_t tick) {
    std::ostringstream receipt;
    receipt << "{\n";
    receipt << "  \"commandId\": \"" << CommandSpoolJsonEscape(command_file.command_id) << "\",\n";
    receipt << "  \"status\": \"" << CommandSpoolJsonEscape(status) << "\",\n";
    receipt << "  \"message\": \"" << CommandSpoolJsonEscape(message) << "\",\n";
    receipt << "  \"tick\": " << tick << ",\n";
    receipt << "  \"updatedAt\": \"" << CommandSpoolTimestampUtc() << "\",\n";
    receipt << "  \"source\": \"" << CommandSpoolJsonEscape(command_file.source_path.filename().string()) << "\"\n";
    receipt << "}\n";
    WriteTextFile(command_file.receipt_path, receipt.str());
}

inline void WriteCommandResult(
    const SpoolCommandFile& command_file,
    bool ok,
    const std::string& message,
    std::uint64_t tick) {
    std::ostringstream result;
    result << "{\n";
    result << "  \"commandId\": \"" << CommandSpoolJsonEscape(command_file.command_id) << "\",\n";
    result << "  \"ok\": " << (ok ? "true" : "false") << ",\n";
    result << "  \"message\": \"" << CommandSpoolJsonEscape(message) << "\",\n";
    result << "  \"tick\": " << tick << ",\n";
    result << "  \"completedAt\": \"" << CommandSpoolTimestampUtc() << "\",\n";
    result << "  \"source\": \"" << CommandSpoolJsonEscape(command_file.source_path.filename().string()) << "\"\n";
    result << "}\n";
    WriteTextFile(command_file.result_path, result.str());
}

inline void WriteCommandBatchResult(
    const SpoolCommandFile& command_file,
    bool ok,
    const std::string& message,
    std::uint64_t tick,
    const std::vector<std::pair<std::string, std::string>>& command_results) {
    std::ostringstream result;
    result << "{\n";
    result << "  \"commandId\": \"" << CommandSpoolJsonEscape(command_file.command_id) << "\",\n";
    result << "  \"ok\": " << (ok ? "true" : "false") << ",\n";
    result << "  \"message\": \"" << CommandSpoolJsonEscape(message) << "\",\n";
    result << "  \"tick\": " << tick << ",\n";
    result << "  \"completedAt\": \"" << CommandSpoolTimestampUtc() << "\",\n";
    result << "  \"source\": \"" << CommandSpoolJsonEscape(command_file.source_path.filename().string()) << "\",\n";
    result << "  \"commands\": [\n";
    for (std::size_t i = 0; i < command_results.size(); ++i) {
        result << "    {\"commandId\": \"" << CommandSpoolJsonEscape(command_results[i].first)
               << "\", \"message\": \"" << CommandSpoolJsonEscape(command_results[i].second) << "\"}";
        if (i + 1 < command_results.size()) {
            result << ',';
        }
        result << "\n";
    }
    result << "  ]\n";
    result << "}\n";
    WriteTextFile(command_file.result_path, result.str());
}

inline std::vector<std::filesystem::path> ListCommandInboxFiles(const std::filesystem::path& inbox) {
    std::vector<std::filesystem::path> paths;
    if (!std::filesystem::exists(inbox)) {
        return paths;
    }
    for (const auto& entry : std::filesystem::directory_iterator(inbox)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
            continue;
        }
        paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

inline std::optional<SpoolCommandFile> ClaimCommandFile(
    const std::filesystem::path& source_path,
    const SpoolDirectories& directories) {
    const std::string base_name = source_path.stem().string();
    std::string command_id = SanitizeCommandId(base_name);
    if (command_id.empty()) {
        command_id = "command";
    }
    const std::string timestamp = CommandSpoolFileTimestampUtc();
    const std::string unique_prefix = command_id + "-" + timestamp;
    const std::filesystem::path claimed_path = NextAvailableSpoolPath(
        directories.processing,
        unique_prefix,
        source_path.extension().string());

    std::error_code rename_error;
    std::filesystem::rename(source_path, claimed_path, rename_error);
    if (rename_error) {
        return std::nullopt;
    }

    const auto payload = ReadTextFile(claimed_path);
    if (!payload.has_value()) {
        return std::nullopt;
    }

    SpoolCommandFile command_file;
    command_file.source_path = source_path;
    command_file.claimed_path = claimed_path;
    command_file.command_id = claimed_path.stem().string();
    command_file.receipt_path = NextAvailableSpoolPath(directories.receipts, command_file.command_id, ".json").string();
    command_file.result_path = NextAvailableSpoolPath(directories.results, command_file.command_id, ".json").string();
    command_file.payload = *payload;
    return command_file;
}

inline void ArchiveClaimedCommandFile(
    const SpoolCommandFile& command_file,
    const SpoolDirectories& directories) {
    const std::filesystem::path archive_path = NextAvailableSpoolPath(
        directories.archive,
        command_file.claimed_path.stem().string(),
        command_file.claimed_path.extension().string());
    std::error_code rename_error;
    std::filesystem::rename(command_file.claimed_path, archive_path, rename_error);
    if (!rename_error) {
        return;
    }

    std::error_code copy_error;
    std::filesystem::copy_file(
        command_file.claimed_path,
        archive_path,
        std::filesystem::copy_options::overwrite_existing,
        copy_error);
    if (!copy_error) {
        std::error_code remove_error;
        std::filesystem::remove(command_file.claimed_path, remove_error);
    }
}

}  // namespace dbd_server
