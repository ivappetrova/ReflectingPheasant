#pragma once

#include <string>
#include <string_view>
#include <fstream>
#include <iostream>
#include <charconv>
#include <system_error>

namespace FlyTracer {

namespace detail {

[[nodiscard]] inline std::string_view trim(std::string_view str) noexcept {
    const auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return {};
    const auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

template<typename T>
bool parseNumber(std::string_view str, T& out) noexcept {
    const auto* begin = str.data();
    const auto* end = str.data() + str.size();
    auto [ptr, ec] = std::from_chars(begin, end, out);
    return ec == std::errc{} && ptr == end;
}

inline bool parseFloat(std::string_view str, float& out) noexcept {
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
    const auto* begin = str.data();
    const auto* end = str.data() + str.size();
    auto [ptr, ec] = std::from_chars(begin, end, out);
    return ec == std::errc{};
#else
    // Fallback for older implementations (some libstdc++ versions)
    try {
        out = std::stof(std::string(str));
        return true;
    } catch (...) {
        return false;
    }
#endif
}

} // namespace detail

struct AppConfig {
    // Window settings
    int windowWidth{1280};
    int windowHeight{720};
    std::string windowTitle{"FlyTracer"};
    bool fullscreen{false};
    bool vsync{true};

    // Raytracing settings
    int maxRayBounces{3};
    int samplesPerPixel{1};
    float shadowBias{0.001f};

    // Resource paths
    std::string resourceDirectory{"resources"};
    std::string shaderDirectory{"shaders"};
    std::string defaultModel{"pheasant.obj"};

    // Camera defaults
    float cameraFov{60.0f};
    float cameraNear{0.1f};
    float cameraFar{100.0f};

    [[nodiscard]] bool loadFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file) {
            std::cerr << "Config: Could not open " << filename << ", using defaults\n";
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            const auto trimmedLine = detail::trim(line);

            // Skip empty lines and comments
            if (trimmedLine.empty() || trimmedLine[0] == '#' || trimmedLine[0] == ';') {
                continue;
            }

            // Find the = separator
            const auto pos = trimmedLine.find('=');
            if (pos == std::string_view::npos) {
                continue;
            }

            const auto key = detail::trim(trimmedLine.substr(0, pos));
            const auto value = detail::trim(trimmedLine.substr(pos + 1));

            parseConfigLine(key, value);
        }

        std::cout << "Config: Loaded settings from " << filename << "\n";
        return true;
    }

    [[nodiscard]] bool saveToFile(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file) {
            std::cerr << "Config: Could not write to " << filename << "\n";
            return false;
        }

        file << "# FlyTracer Configuration File\n"
             << "# Generated automatically\n\n"
             << "# Window Settings\n"
             << "window_width=" << windowWidth << "\n"
             << "window_height=" << windowHeight << "\n"
             << "window_title=" << windowTitle << "\n"
             << "fullscreen=" << (fullscreen ? "true" : "false") << "\n"
             << "vsync=" << (vsync ? "true" : "false") << "\n\n"
             << "# Raytracing Settings\n"
             << "max_ray_bounces=" << maxRayBounces << "\n"
             << "samples_per_pixel=" << samplesPerPixel << "\n"
             << "shadow_bias=" << shadowBias << "\n\n"
             << "# Resource Paths\n"
             << "resource_directory=" << resourceDirectory << "\n"
             << "shader_directory=" << shaderDirectory << "\n"
             << "default_model=" << defaultModel << "\n\n"
             << "# Camera Defaults\n"
             << "camera_fov=" << cameraFov << "\n"
             << "camera_near=" << cameraNear << "\n"
             << "camera_far=" << cameraFar << "\n";

        std::cout << "Config: Saved settings to " << filename << "\n";
        return true;
    }

private:
    void parseConfigLine(std::string_view key, std::string_view value) noexcept {
        // Window settings
        if (key == "window_width") {
            detail::parseNumber(value, windowWidth);
        } else if (key == "window_height") {
            detail::parseNumber(value, windowHeight);
        } else if (key == "window_title") {
            windowTitle = std::string(value);
        } else if (key == "fullscreen") {
            fullscreen = (value == "true" || value == "1");
        } else if (key == "vsync") {
            vsync = (value == "true" || value == "1");
        }
        // Raytracing settings
        else if (key == "max_ray_bounces") {
            detail::parseNumber(value, maxRayBounces);
        } else if (key == "samples_per_pixel") {
            detail::parseNumber(value, samplesPerPixel);
        } else if (key == "shadow_bias") {
            detail::parseFloat(value, shadowBias);
        }
        // Resource paths
        else if (key == "resource_directory") {
            resourceDirectory = std::string(value);
        } else if (key == "shader_directory") {
            shaderDirectory = std::string(value);
        } else if (key == "default_model") {
            defaultModel = std::string(value);
        }
        // Camera settings
        else if (key == "camera_fov") {
            detail::parseFloat(value, cameraFov);
        } else if (key == "camera_near") {
            detail::parseFloat(value, cameraNear);
        } else if (key == "camera_far") {
            detail::parseFloat(value, cameraFar);
        }
    }
};

} // namespace FlyTracer
