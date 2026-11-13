require "json"

Pod::Spec.new do |s|
  package = JSON.parse(File.read(File.join(__dir__, "package.json")))
  s.name         = "ReactNativeSmb"
  s.version      = package.fetch("version")
  s.summary      = "A React Native Nitro module for SMB client functionality using libsmb2"
  s.description  = <<-DESC
    A SMB client library for React Native applications using the libsmb2 C library, built using Nitro modules.
  DESC

  s.homepage     = "https://github.com/cetapod/react-native-smb"
  s.author       = { "Cetapod" => "open-source@cetapod.com" }
  s.platform     = :ios, "12.0"

  s.source       = { :git => "https://github.com/cetapod/react-native-smb.git", :tag => "v#{s.version}" }

  s.source_files = [
    "cpp/shared/**/*.{cpp,hpp}"
  ]

  s.header_mappings_dir = "cpp"
  s.dependency "React"
  s.dependency "React-Core"
  s.dependency "NitroModules"

  # Transfer, listing, and tree orchestration headers are implementation details.
  s.public_header_files = Dir["cpp/**/*.hpp"].reject do |header|
    header.start_with?("cpp/shared/io/transfer/") || header.start_with?("cpp/shared/io/listing/") ||
      header == "cpp/shared/io/SmbCopyTree.hpp" || header == "cpp/shared/io/SmbTreeOps.hpp"
  end

  s.requires_arc = true
  # FOLLY_NO_CONFIG skips folly-config.h; under C++20 that turns FOLLY_HAS_COROUTINES
  # on unless FOLLY_CFG_NO_COROUTINES is set. ReactNativeDependencies prebuilts omit
  # folly/coro headers, so libraries that set FOLLY_NO_CONFIG must also disable coroutines.
  s.pod_target_xcconfig = {
    'GCC_PREPROCESSOR_DEFINITIONS' => '$(inherited) FOLLY_NO_CONFIG FOLLY_MOBILE=1 FOLLY_USE_LIBCPP=1 FOLLY_CFG_NO_COROUTINES=1',
    'HEADER_SEARCH_PATHS' => [
      '$(inherited)',
      '$(PODS_TARGET_SRCROOT)/cpp',
      '$(PODS_TARGET_SRCROOT)/cpp/shared/bridge',
      '$(PODS_TARGET_SRCROOT)/nitrogen/generated/shared/c++',
      '$(PODS_TARGET_SRCROOT)/nitrogen/generated/ios',
      '$(PODS_TARGET_SRCROOT)/libsmb2/include',
      '$(PODS_ROOT)/NitroModules',
      '$(PODS_ROOT)/Headers/Private/NitroModules'
    ].join(' '),
    'OTHER_CPLUSPLUSFLAGS' => '$(inherited) -std=c++20 -Wall -DFOLLY_NO_CONFIG -DFOLLY_MOBILE=1 -DFOLLY_USE_LIBCPP=1 -DFOLLY_CFG_NO_COROUTINES=1'
  }

  s.user_target_xcconfig = {
    'HEADER_SEARCH_PATHS' => [
      '$(inherited)',
      '$(PODS_ROOT)/ReactNativeSmb/cpp',
      '$(PODS_TARGET_SRCROOT)/libsmb2/include',
      '$(PODS_ROOT)/Headers/Private/NitroModules'
    ].join(' ')
  }

  s.vendored_frameworks = "ios/libs/libsmb2.xcframework"

  # Nitrogen-generated sources + iOS autolinking registration
  load 'nitrogen/generated/ios/ReactNativeSmb+autolinking.rb'
  add_nitrogen_files(s)

  # HybridObjects are C++-only. Nitrogen still emits Swift scaffolding that
  # `import NitroModules`; building that Swift module against RN prebuilt Folly
  # fails with missing folly/coro headers. Registration lives in the .mm +load.
  s.exclude_files = 'nitrogen/generated/**/*.swift'
end
