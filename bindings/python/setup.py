"""
Setup script for april_asr. Before running this, you must have compiled the C
library dll or so
"""


import os
import shutil
import glob
import platform
import setuptools

april_build = os.getenv("APRIL_BUILD", os.path.abspath(os.path.join(os.path.dirname(__file__),
    "../../build")))
system = os.environ.get('APRIL_SYSTEM', platform.system())
architecture = os.environ.get('APRIL_ARCHITECTURE', platform.architecture()[0])
machine = os.environ.get('APRIL_MACHINE', platform.machine())

# Copy precompiled libraries
PRECOMP_LIBS = [
    "libaprilasr.so",
    "Release/libaprilasr.dll",
    "../lib/lib/libonnxruntime.so",
    "../lib/lib/onnxruntime.dll"
]

for l in PRECOMP_LIBS:
    for lib in glob.glob(os.path.join(april_build, l)):
        print("Adding library", lib)
        shutil.copy(lib, "april_asr")

# Ensure has the correct suffix (e.g. libonnxruntime.so.1.13.1)
for lib in glob.glob("../lib/lib/libonnxruntime.so.*"):
    shutil.move(
        "april_asr/libonnxruntime.so",
        "april_asr/libonnxruntime.so" + lib.split("libonnxruntime.so")[1]
    )

# Create OS-dependent, but Python-independent wheels.
try:
    from wheel.bdist_wheel import bdist_wheel
except ImportError:
    cmdclass = {}
else:
    # pylint: disable-next=invalid-name,missing-class-docstring
    class bdist_wheel_tag_name(bdist_wheel):
        def get_tag(self):
            abi = 'none'
            if system == 'Darwin':
                oses = 'macosx_10_6_universal2'
            elif system == 'Windows' and architecture == '32bit':
                oses = 'win32'
            elif system == 'Windows' and architecture == '64bit':
                oses = 'win_amd64'
            elif system == 'Linux' and machine == 'aarch64' and architecture == '64bit':
                oses = 'manylinux2014_aarch64'
            elif system == 'Linux':
                oses = 'linux_' + machine
            else:
                raise TypeError("Unknown build environment")
            return 'py3', abi, oses
    cmdclass = {'bdist_wheel': bdist_wheel_tag_name}

with open("README.md", "rb") as fh:
    long_description = fh.read().decode("utf-8")

setuptools.setup(
    name="april_asr",
    version="0.0.3",
    author="abb128",
    author_email="april@sapples.net",
    description="Offline open source speech recognition API based on next-generation Kaldi",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/abb128/april-asr",
    packages=setuptools.find_packages(),
    package_data = {'april_asr': ['*.so', '*.dll', '*.dyld']},
    entry_points = {
        'console_scripts': ['april-transcriber=april_asr.example:main'],
    },
    include_package_data=True,
    classifiers=[
        'Programming Language :: Python :: 3',
        'Operating System :: Microsoft :: Windows',
        'Operating System :: POSIX :: Linux',
        #'Operating System :: MacOS :: MacOS X',
        'Topic :: Software Development :: Libraries :: Python Modules'
    ],
    cmdclass=cmdclass,
    python_requires='>=3',
    zip_safe=False, # Since we load so file from the filesystem, we can not run from zip file
    setup_requires=[],
    install_requires=["librosa"]
)
