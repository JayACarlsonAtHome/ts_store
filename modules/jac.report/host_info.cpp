module;

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <unistd.h>

#include <sys/sysinfo.h>
#include <sys/utsname.h>

module jac.report;

namespace fs = std::filesystem;

namespace {

std::string trim(std::string s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string read_os_pretty() {
    std::ifstream in("/etc/os-release");
    if (!in) return {};
    std::string line;
    while (std::getline(in, line)) {
        if (line.starts_with("PRETTY_NAME=")) {
            std::string v = line.substr(std::string("PRETTY_NAME=").size());
            if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
                v = v.substr(1, v.size() - 2);
            }
            return v;
        }
    }
    return {};
}

void read_cpuinfo(std::string& model, int& mhz_max, int& physical_cores) {
    std::ifstream in("/proc/cpuinfo");
    if (!in) return;

    std::set<int> physical_ids;
    int line_mhz = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.starts_with("model name") && model.empty()) {
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                model = trim(line.substr(colon + 1));
            }
        } else if (line.starts_with("cpu MHz")) {
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                try {
                    line_mhz = static_cast<int>(std::stod(trim(line.substr(colon + 1))) + 0.5);
                    if (line_mhz > mhz_max) mhz_max = line_mhz;
                } catch (...) {
                }
            }
        } else if (line.starts_with("physical id")) {
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                try {
                    physical_ids.insert(std::stoi(trim(line.substr(colon + 1))));
                } catch (...) {
                }
            }
        }
    }
    if (!physical_ids.empty()) {
        physical_cores = static_cast<int>(physical_ids.size());
    }
}

}  // namespace

HostInfo collect_host_info() {
    HostInfo h;

    char hostname[256] = {};
    if (gethostname(hostname, sizeof(hostname) - 1) == 0) {
        h.hostname = hostname;
    }

    struct utsname uts{};
    if (uname(&uts) == 0) {
        h.arch = uts.machine;
        if (h.os_pretty.empty()) {
            h.os_pretty = uts.sysname;
            if (uts.release[0] != '\0') {
                h.os_pretty += " ";
                h.os_pretty += uts.release;
            }
        }
    }
    const std::string pretty = read_os_pretty();
    if (!pretty.empty()) {
        h.os_pretty = pretty;
    }

    h.logical_cores = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    if (h.logical_cores < 0) h.logical_cores = 0;

    read_cpuinfo(h.cpu_model, h.cpu_mhz_max, h.physical_cores);
    if (h.physical_cores <= 0 && h.logical_cores > 0) {
        h.physical_cores = h.logical_cores;
    }

    struct sysinfo si{};
    if (sysinfo(&si) == 0) {
        const std::uint64_t unit = si.mem_unit ? static_cast<std::uint64_t>(si.mem_unit) : 1ULL;
        h.ram_total_mib = (static_cast<std::uint64_t>(si.totalram) * unit) / (1024ULL * 1024ULL);
        h.ram_avail_mib = ((static_cast<std::uint64_t>(si.freeram)
                            + static_cast<std::uint64_t>(si.bufferram)) * unit)
                          / (1024ULL * 1024ULL);
    }

    return h;
}

bool write_os_info_txt(const fs::path& results_base,
                       const HostInfo& host,
                       const std::string& os_id) {
    fs::create_directories(results_base);
    fs::path path = results_base / "OS_INFO.txt";
    std::ofstream out(path);
    if (!out) return false;

    out << "# ts_store host snapshot (captured at run start)\n";
    if (!os_id.empty()) out << "OS_ID=" << os_id << "\n";
    if (!host.hostname.empty()) out << "Hostname=" << host.hostname << "\n";
    if (!host.os_pretty.empty()) out << "OS=" << host.os_pretty << "\n";
    if (!host.arch.empty()) out << "Arch=" << host.arch << "\n";
    if (!host.cpu_model.empty()) out << "CPU=" << host.cpu_model << "\n";
    if (host.logical_cores > 0) out << "LogicalCores=" << host.logical_cores << "\n";
    if (host.physical_cores > 0) out << "PhysicalCores=" << host.physical_cores << "\n";
    if (host.cpu_mhz_max > 0) out << "CpuMhzMax=" << host.cpu_mhz_max << "\n";
    if (host.ram_total_mib > 0) out << "RamTotalMiB=" << host.ram_total_mib << "\n";
    if (host.ram_avail_mib > 0) out << "RamAvailMiB=" << host.ram_avail_mib << "\n";
    return static_cast<bool>(out);
}

std::string host_info_summary_line(const HostInfo& host) {
    if (host.logical_cores <= 0 && host.ram_total_mib == 0) return {};

    std::string s;
    if (host.logical_cores > 0) {
        s += std::to_string(host.logical_cores);
        s += " cores";
        if (host.physical_cores > 0 && host.physical_cores != host.logical_cores) {
            s += " (";
            s += std::to_string(host.physical_cores);
            s += " physical)";
        }
    }
    if (host.ram_total_mib > 0) {
        if (!s.empty()) s += ", ";
        s += format_locale_int(host.ram_total_mib);
        s += " MiB RAM";
    }
    if (!host.cpu_model.empty()) {
        if (!s.empty()) s += " — ";
        s += host.cpu_model;
        if (host.cpu_mhz_max > 0) {
            s += " @ ";
            s += format_locale_int(host.cpu_mhz_max);
            s += " MHz max";
        }
    }
    return s;
}