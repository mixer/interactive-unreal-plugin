nuget restore %~dp0\interactive-cpp\cpprestsdk\release\src\build\vs14.static\packages.config -PackagesDirectory %~dp0\interactive-cpp\External\Packages

msbuild %~dp0\interactive-cpp\cpprestsdk\release\src\build\vs14.static\casablanca140.static.vcxproj /p:Configuration=Release /p:Platform=x64 /p:"PackagesRoot=%~dp0\interactive-cpp\External\Packages"
msbuild interactive-cpp\cpprestsdk\release\src\build\vs14.static\casablanca140.static.vcxproj /p:Configuration=Release /p:Platform=Win32 /p:"PackagesRoot=%~dp0\interactive-cpp\External\Packages"

msbuild %~dp0\interactive-cpp\Build\Interactivity.Win32.Cpp\Interactivity.Win32.Cpp.vcxproj /p:Configuration=Release /p:Platform=x64
msbuild %~dp0\interactive-cpp\Build\Interactivity.Win32.Cpp\Interactivity.Win32.Cpp.vcxproj /p:Configuration=Release /p:Platform=Win32

msbuild %~dp0\interactive-cpp\cpprestsdk\release\src\build\vs14.xbox\casablanca140.xbox.vcxproj /p:Configuration=Release /p:Platform=Durango

msbuild %~dp0\interactive-cpp\Build\Interactivity.Xbox.Cpp\Interactivity.Xbox.Cpp.vcxproj /p:Configuration=Release /p:Platform=Durango

msbuild %~dp0\interactive-cpp\cpprestsdk\release\src\build\vs14.uwp\cpprestsdk140.uwp.static.vcxproj /p:Configuration=Release /p:Platform=x64
msbuild interactive-cpp\cpprestsdk\release\src\build\vs14.uwp\cpprestsdk140.uwp.static.vcxproj /p:Configuration=Release /p:Platform=Win32

msbuild %~dp0\interactive-cpp\Build\Interactivity.UWP.Cpp\Interactivity.UWP.Cpp.vcxproj /p:Configuration=Release /p:Platform=x64
msbuild %~dp0\interactive-cpp\Build\Interactivity.UWP.Cpp\Interactivity.UWP.Cpp.vcxproj /p:Configuration=Release /p:Platform=Win32
