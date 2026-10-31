@echo off 
cd /d E:\GitRepos\VamlPOC\build\msvc-debug 
ctest -R vaml-core-tests --output-on-failure 
exit /b %0%
