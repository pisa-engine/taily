from conans import ConanFile, tools, CMake
import os


class TailyConan(ConanFile):
    name = "taily"
    version = "0.1"
    license = "MIT License"
    description = "Implementation of Taily algorithm"
    url = "https://github.com/elshize/taily"
    code_url = "https://github.com/elshize/taily"
    build_policy="missing"
    generators = "cmake_paths", "ycm"
    options = {"use_system_boost": [True, False]}
    default_options = ("use_system_boost=False")
    exports_sources = ("LICENSE", "README.md", "include/*", "examples/*",
                       "cmake/*", "CMakeLists.txt", "test/*")

    def package(self):
        cmake = CMake(self)
        cmake.definitions["CMAKE_TOOLCHAIN_FILE"] = 'conan_paths.cmake'
        cmake.configure()
        self.copy("*", dst="include", src="include")
        self.copy("tailyConfig.cmake")
        self.copy("tailyConfigVersion.cmake")
        self.copy("tailyTargets.cmake")

    def requirements(self):
        if not self.options.use_system_boost:
            self.requires("boost/1.66.0@conan/stable")
