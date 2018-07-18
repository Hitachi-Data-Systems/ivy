from setuptools import setup

setup(
    name="Ivy",
    version="2.01.01",
    description="Ivy Engine REST Client",
    keywords=["hitachi", "storage", "ivy", "rest", "client"],
    url="https://github.com/Hitachi-Data-Systems/ivy",
    download_url="https://github.com/Hitachi-Data-Systems/rest-client/archive/1.14.0.tar.gz",
    author="Hitachi Vantara",
    author_email = "kumaran.subramaniam@hitachivantara.com",
    license="Apache License Version 2.0",
    packages=["ivyrest"],
    install_requires=["requests"],
    tests_require=['mock'],
)
