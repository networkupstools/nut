"""
The setup.py file for PyNUTClient
"""
# Based on https://medium.com/@VersuS_/automate-pypi-releases-with-github-actions-4c5a9cfe947d
# See also .github/workflows/PyNUTClient.yml for active packaging steps

from setuptools import setup, find_packages
import codecs
import os

here = os.path.abspath(os.path.dirname(__file__))

with codecs.open(os.path.join(here, "README.md"), encoding="utf-8") as fh:
    long_description = "\\n" + fh.read()

setup(
    name="PyNUTClient",
    version='{{VERSION_PLACEHOLDER}}',
    author="The Network UPS Tools project",
    author_email="jimklimov+nut@gmail.com",
    description="Python client bindings for NUT",
    url = "https://github.com/networkupstools/nut/tree/master/scripts/python/module",
    long_description_content_type="text/asciidoc",
    long_description=long_description,
    packages=find_packages(),
    install_requires=['telnetlib'],
    keywords=['pypi', 'cicd', 'python'],
    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "Intended Audience :: Developers",
        "Programming Language :: Python :: 2.6",
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 3",
        "Operating System :: Unix",
        "Operating System :: MacOS :: MacOS X",
        "Operating System :: Microsoft :: Windows"
    ]
)

