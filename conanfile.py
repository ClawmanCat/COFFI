from conans import ConanFile


class COFFIConan(ConanFile):
    name = "COFFI"
    description = "A header-only C++ library for accessing files in COFF binary format. (Including Windows PE/PE+ formats)"
    url = "https://github.com/serge1/COFFI"
    homepage = url
    author = "serge1"
    license = "MIT"
    exports_sources = ["PE/*", "coffi/*"]
    no_copy_source = True

    def package(self):
        self.copy(pattern="*", dst="include/PE", src="PE", keep_path=True)
        self.copy(pattern="*", dst="include/coffi", src="coffi", keep_path=True)

    def package_info(self):
        if not self.in_local_cache:
            self.cpp_info.includedirs = ["PE", "coffi"]

    def package_id(self):
        self.info.header_only()