#include "TemplateConfig.h"

#include "Utils/ImageIo.h"
#include "Utils/Logger.h"

MAA_RES_NS_BEGIN

bool TemplateConfig::lazy_load(const std::string& name, const std::filesystem::path& root,
                               const std::vector<std::string>& filenames)
{
    LogDebug << VAR(name) << VAR(root) << VAR(filenames);

    auto& lazy_paths = template_paths_[name];
    lazy_paths.roots.emplace_back(root);
    lazy_paths.filenames = filenames;

    template_cache_.erase(name);

#ifdef MAA_DEBUG
    const auto& images = get_template_images(name);
    if (images.empty() || MAA_RNS::ranges::any_of(images, [](const auto& image) -> bool { return image.empty(); })) {
        LogError << "image is empty" << VAR(name) << VAR(images);
        return false;
    }
#endif

    return true;
}

void TemplateConfig::clear()
{
    LogFunc;

    template_paths_.clear();
    template_cache_.clear();
    template_bank_.clear();
}

const std::vector<cv::Mat>& TemplateConfig::get_template_images(const std::string& name) const
{
    if (auto templ_iter = template_cache_.find(name); templ_iter != template_cache_.end()) {
        return templ_iter->second;
    }

    return load_images(name);
}

const std::vector<cv::Mat>& TemplateConfig::load_images(const std::string& name) const
{
    LogFunc << VAR(name);

    auto path_iter = template_paths_.find(name);
    if (path_iter == template_paths_.end()) {
        LogError << "Invalid template name" << VAR(name);
        static std::vector<cv::Mat> empty;
        return empty;
    }
    const Paths& paths = path_iter->second;

    std::vector<cv::Mat> images;
    for (const auto& filename : paths.filenames) {
        cv::Mat templ_mat;
        for (const auto& root : paths.roots | MAA_RNS::views::reverse) {
            auto path = root / MAA_NS::path(filename);
            templ_mat = load_single_image(path);
            if (!templ_mat.empty()) {
                break;
            }
        }
        if (templ_mat.empty()) {
            LogError << "template image is empty" << VAR(name) << VAR(filename) << VAR(paths.roots);
#ifdef MAA_DEBUG
            static std::vector<cv::Mat> empty;
            return empty;
#else
            continue;
#endif
        }

        images.emplace_back(std::move(templ_mat));
    }

    if (images.empty()) {
        LogError << "template list is empty" << VAR(name) << VAR(paths.filenames) << VAR(paths.roots);
        static std::vector<cv::Mat> empty;
        return empty;
    }

    return template_cache_.emplace(name, std::move(images)).first->second;
}

cv::Mat TemplateConfig::load_single_image(const std::filesystem::path& path) const
{
    if (auto bank_iter = template_bank_.find(path); bank_iter != template_bank_.end()) {
        LogDebug << "Withdraw image" << VAR(path);
        return bank_iter->second;
    }
    else if (std::filesystem::exists(path)) {
        LogDebug << "Read image" << VAR(path);
        cv::Mat temp = MAA_NS::imread(path);
        template_bank_.emplace(path, temp);
        return temp;
    }

    LogDebug << "Image not found" << VAR(path);
    return {};
}

MAA_RES_NS_END
