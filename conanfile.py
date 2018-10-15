from conans import ConanFile, tools
import os


class TailyConan(ConanFile):
    name = "taily"
    version = "0.1"
    license = "MIT License"
    description = "Implementation of Taily algorithm"
    url = "https://github.com/elshize/taily"
    code_url = "https://github.com/elshize/taily"
    build_policy="missing"
    exports_sources = ("LICENSE", "README.md", "include/*",
                       "cmake/*", "CMakeLists.txt", "test/*")

    def package(self):
        self.copy("*.hpp", dst="include", src="include")
