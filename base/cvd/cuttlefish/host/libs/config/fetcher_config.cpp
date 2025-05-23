/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cuttlefish/host/libs/config/fetcher_config.h"

#include <cctype>
#include <fstream>
#include <map>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <gflags/gflags.h>
#include <json/reader.h>
#include <json/value.h>
#include <json/writer.h>

#include "cuttlefish/common/libs/utils/files.h"
#include "cuttlefish/common/libs/utils/result.h"

namespace cuttlefish {

namespace {

const char* kFlags = "flags";
const char* kCvdFiles = "cvd_files";
const char* kCvdFileSource = "source";
const char* kCvdFileBuildId = "build_id";
const char* kCvdFileBuildTarget = "build_target";

FileSource SourceStringToEnum(std::string source) {
  for (auto& c : source) {
    c = std::tolower(c);
  }
  if (source == "default_build") {
    return FileSource::DEFAULT_BUILD;
  } else if (source == "system_build") {
    return FileSource::SYSTEM_BUILD;
  } else if (source == "kernel_build") {
    return FileSource::KERNEL_BUILD;
  } else if (source == "local_file") {
    return FileSource::LOCAL_FILE;
  } else if (source == "generated") {
    return FileSource::GENERATED;
  } else if (source == "bootloader_build") {
    return FileSource::BOOTLOADER_BUILD;
  } else if (source == "android_efi_loader_build") {
    return FileSource::ANDROID_EFI_LOADER_BUILD;
  } else if (source == "boot_build") {
    return FileSource::BOOT_BUILD;
  } else if (source == "host_package_build") {
    return FileSource::HOST_PACKAGE_BUILD;
  } else if (source == "chrome_os_build") {
    return FileSource::CHROME_OS_BUILD;
  } else {
    return FileSource::UNKNOWN_PURPOSE;
  }
}

std::string SourceEnumToString(const FileSource& source) {
  if (source == FileSource::DEFAULT_BUILD) {
    return "default_build";
  } else if (source == FileSource::SYSTEM_BUILD) {
    return "system_build";
  } else if (source == FileSource::KERNEL_BUILD) {
    return "kernel_build";
  } else if (source == FileSource::LOCAL_FILE) {
    return "local_file";
  } else if (source == FileSource::GENERATED) {
    return "generated";
  } else if (source == FileSource::BOOTLOADER_BUILD) {
    return "bootloader_build";
  } else if (source == FileSource::ANDROID_EFI_LOADER_BUILD) {
    return "android_efi_loader_build";
  } else if (source == FileSource::BOOT_BUILD) {
    return "boot_build";
  } else if (source == FileSource::HOST_PACKAGE_BUILD) {
    return "host_package_build";
  } else if (source == FileSource::CHROME_OS_BUILD) {
    return "chrome_os_build";
  } else {
    return "unknown";
  }
}

} // namespace

CvdFile::CvdFile() {
}

CvdFile::CvdFile(const FileSource& source, const std::string& build_id,
                 const std::string& build_target, const std::string& file_path)
    : source(source), build_id(build_id), build_target(build_target), file_path(file_path) {
}

std::ostream& operator<<(std::ostream& os, const CvdFile& cvd_file) {
  os << "CvdFile(";
  os << "source = " << SourceEnumToString(cvd_file.source) << ", ";
  os << "build_id = " << cvd_file.build_id << ", ";
  os << "build_target = " << cvd_file.build_target << ", ";
  os << "file_path = " << cvd_file.file_path << ")";
  return os;
}

FetcherConfig::FetcherConfig() : dictionary_(new Json::Value()) {
}

FetcherConfig::FetcherConfig(FetcherConfig&&) = default;

FetcherConfig::~FetcherConfig() {
}

bool FetcherConfig::SaveToFile(const std::string& file) const {
  std::ofstream ofs(file);
  if (!ofs.is_open()) {
    LOG(ERROR) << "Unable to write to file " << file;
    return false;
  }
  ofs << *dictionary_;
  return !ofs.fail();
}

bool FetcherConfig::LoadFromFile(const std::string& file) {
  auto real_file_path = AbsolutePath(file);
  if (real_file_path.empty()) {
    LOG(ERROR) << "Could not get real path for file " << file;
    return false;
  }
  Json::CharReaderBuilder builder;
  std::ifstream ifs(real_file_path);
  std::string errorMessage;
  if (!Json::parseFromStream(builder, ifs, dictionary_.get(), &errorMessage)) {
    LOG(ERROR) << "Could not read config file " << file << ": " << errorMessage;
    return false;
  }

  auto base_dir = android::base::Dirname(file);
  if (base_dir != "." && dictionary_->isMember(kCvdFiles)) {
    LOG(INFO) << "Adjusting cvd_file paths to directory: " << base_dir;
    for (const auto& member_name : (*dictionary_)[kCvdFiles].getMemberNames()) {
      (*dictionary_)[kCvdFiles][base_dir + "/" + member_name] =
          (*dictionary_)[kCvdFiles][member_name];
      (*dictionary_)[kCvdFiles].removeMember(member_name);
    }
  }

  return true;
}

void FetcherConfig::RecordFlags() {
  std::vector<gflags::CommandLineFlagInfo> all_flags;
  GetAllFlags(&all_flags);
  Json::Value flags_json(Json::arrayValue);
  for (const auto& flag : all_flags) {
    Json::Value flag_json;
    flag_json["name"] = flag.name;
    flag_json["type"] = flag.type;
    flag_json["description"] = flag.description;
    flag_json["current_value"] = flag.current_value;
    flag_json["default_value"] = flag.default_value;
    flag_json["filename"] = flag.filename;
    flag_json["has_validator_fn"] = flag.has_validator_fn;
    flag_json["is_default"] = flag.is_default;
    flags_json.append(flag_json);
  }
  (*dictionary_)[kFlags] = flags_json;
}

namespace {

CvdFile JsonToCvdFile(const std::string& file_path, const Json::Value& json) {
  CvdFile cvd_file;
  cvd_file.file_path = file_path;
  if (json.isMember(kCvdFileSource)) {
    cvd_file.source = SourceStringToEnum(json[kCvdFileSource].asString());
  } else {
    cvd_file.source = FileSource::UNKNOWN_PURPOSE;
  }
  if (json.isMember(kCvdFileBuildId)) {
    cvd_file.build_id = json[kCvdFileBuildId].asString();
  }
  if (json.isMember(kCvdFileBuildTarget)) {
    cvd_file.build_target = json[kCvdFileBuildTarget].asString();
  }
  return cvd_file;
}

Json::Value CvdFileToJson(const CvdFile& cvd_file) {
  Json::Value json;
  json[kCvdFileSource] = SourceEnumToString(cvd_file.source);
  json[kCvdFileBuildId] = cvd_file.build_id;
  json[kCvdFileBuildTarget] = cvd_file.build_target;
  return json;
}

} // namespace

bool FetcherConfig::add_cvd_file(const CvdFile& file, bool override_entry) {
  if (!dictionary_->isMember(kCvdFiles)) {
    Json::Value files_json(Json::objectValue);
    (*dictionary_)[kCvdFiles] = files_json;
  }
  if ((*dictionary_)[kCvdFiles].isMember(file.file_path) && !override_entry) {
    return false;
  }
  (*dictionary_)[kCvdFiles][file.file_path] = CvdFileToJson(file);
  return true;
}

std::map<std::string, CvdFile> FetcherConfig::get_cvd_files() const {
  if (!dictionary_->isMember(kCvdFiles)) {
    return {};
  }
  std::map<std::string, CvdFile> files;
  const auto& json_files = (*dictionary_)[kCvdFiles];
  for (auto it = json_files.begin(); it != json_files.end(); it++) {
    files[it.key().asString()] = JsonToCvdFile(it.key().asString(), *it);
  }
  return files;
}

std::string FetcherConfig::FindCvdFileWithSuffix(const std::string& suffix) const {
  if (!dictionary_->isMember(kCvdFiles)) {
    return {};
  }
  const auto& json_files = (*dictionary_)[kCvdFiles];
  for (auto it = json_files.begin(); it != json_files.end(); it++) {
    const auto& file = it.key().asString();
    if (android::base::EndsWith(file, suffix)) {
      return file;
    }
  }
  LOG(DEBUG) << "Could not find file ending in " << suffix;
  return "";
}

Result<void> FetcherConfig::AddFilesToConfig(
    FileSource purpose, const std::string& build_id,
    const std::string& build_target, const std::vector<std::string>& paths,
    const std::string& directory_prefix, bool override_entry) {
  for (const std::string& path : paths) {
    std::string_view local_path(path);
    if (!android::base::ConsumePrefix(&local_path, directory_prefix)) {
      LOG(ERROR) << "Failed to remove prefix " << directory_prefix << " from "
                 << local_path;
    }
    while (android::base::StartsWith(local_path, "/")) {
      android::base::ConsumePrefix(&local_path, "/");
    }
    // TODO(schuffelen): Do better for local builds here.
    CvdFile file(purpose, build_id, build_target, std::string(local_path));
    CF_EXPECT(add_cvd_file(file, override_entry),
              "Duplicate file \""
                  << file << "\", Existing file: \"" << get_cvd_files()[path]
                  << "\". Failed to add path \"" << path << "\"");
  }
  return {};
}

} // namespace cuttlefish
