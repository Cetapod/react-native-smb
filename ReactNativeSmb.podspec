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
    "cpp/shared/**/*.{cpp,hpp}",
    "cpp/ios/**/*.{cpp,hpp,mm,m,h}"
  ]

  s.header_mappings_dir = "cpp"
  # React Native dependencies
  s.dependency "React"
  s.dependency "React-Core"
  s.dependency "NitroModules"

  # Transfer and tree orchestration headers are implementation details.
  s.public_header_files = Dir["cpp/**/*.hpp"].reject do |header|
    header.start_with?("cpp/shared/io/transfer/") || header == "cpp/shared/io/SmbCopyTree.hpp"
  end

  # Compiler settings
  s.requires_arc = true
  s.pod_target_xcconfig = {
    'GCC_PREPROCESSOR_DEFINITIONS' => '$(inherited) FOLLY_NO_CONFIG FOLLY_MOBILE=1 FOLLY_USE_LIBCPP=1',
    'HEADER_SEARCH_PATHS' => [
      '$(inherited)',
      '$(PODS_TARGET_SRCROOT)/cpp',
      '$(PODS_TARGET_SRCROOT)/libsmb2/include',
      '$(PODS_ROOT)/NitroModules',
      '$(PODS_ROOT)/Headers/Private/NitroModules'
    ].join(' '),
    'OTHER_CPLUSPLUSFLAGS' => '$(inherited) -std=c++20 -Wall'
  }

  s.user_target_xcconfig = {
    'HEADER_SEARCH_PATHS' => [
      '$(inherited)',
      '$(PODS_ROOT)/ReactNativeSmb/cpp',
      '$(PODS_TARGET_SRCROOT)/libsmb2/include',
      '$(PODS_ROOT)/Headers/Private/NitroModules'
    ].join(' ')
  }

  # Modern XCFramework approach - automatically selects correct library
  s.vendored_frameworks = "ios/libs/libsmb2.xcframework"

end
