%~dp0\nuget restore %~dp0\interactive-cpp\cpprestsdk\release\src\build\vs14.static\packages.config -PackagesDirectory %~dp0\interactive-cpp\Tools\Packages

msbuild %~dp0\interactive-cpp\cpprestsdk\release\src\build\vs14.static\casablanca140.static.vcxproj /p:Configuration=Release /p:Platform=x64 /p:ForceImportBeforeCppTargets="%~dp0\TurnOffLtcg.props"
msbuild %~dp0\interactive-cpp\cpprestsdk\release\src\build\vs14.static\casablanca140.static.vcxproj /p:Configuration=Release /p:Platform=Win32 /p:ForceImportBeforeCppTargets="%~dp0\TurnOffLtcg.props

msbuild %~dp0\interactive-cpp\Build\Interactivity.Win32.Cpp\Interactivity.Win32.Cpp.vcxproj /p:Configuration=Release /p:Platform=x64
msbuild %~dp0\interactive-cpp\Build\Interactivity.Win32.Cpp\Interactivity.Win32.Cpp.vcxproj /p:Configuration=Release /p:Platform=Win32

msbuild %~dp0\interactive-cpp\cpprestsdk\release\src\build\vs14.xbox\casablanca140.xbox.vcxproj /p:Configuration=Release /p:Platform=Durango

msbuild %~dp0\interactive-cpp\Build\Interactivity.Xbox.Cpp\Interactivity.Xbox.Cpp.vcxproj /p:Configuration=Release /p:Platform=Durango

msbuild %~dp0\interactive-cpp\cpprestsdk\release\src\build\vs14.uwp\cpprestsdk140.uwp.static.vcxproj /p:Configuration=Release /p:Platform=x64
msbuild %~dp0\interactive-cpp\cpprestsdk\release\src\build\vs14.uwp\cpprestsdk140.uwp.static.vcxproj /p:Configuration=Release /p:Platform=Win32

msbuild %~dp0\interactive-cpp\Build\Interactivity.UWP.Cpp\Interactivity.UWP.Cpp.vcxproj /p:Configuration=Release /p:Platform=x64
msbuild %~dp0\interactive-cpp\Build\Interactivity.UWP.Cpp\Interactivity.UWP.Cpp.vcxproj /p:Configuration=Release /p:Platform=Win32

xcopy /Y /I %~dp0\interactive-cpp\Binaries\Release\Durango\casablanca140.xbox\*.* %~dp0\Lib\XboxOne
xcopy /Y /I %~dp0\interactive-cpp\Binaries\Release\Durango\Interactivity.Xbox.Cpp\*.* %~dp0\Lib\XboxOne
xcopy /Y /I %~dp0\interactive-cpp\Binaries\Release\Win32\Casablanca\*.* %~dp0\Lib\Win32
xcopy /Y /I %~dp0\interactive-cpp\Binaries\Release\Win32\Interactivity.Win32.Cpp\*.* %~dp0\Lib\Win32
xcopy /Y /I %~dp0\interactive-cpp\Binaries\Release\x64\Casablanca\*.* %~dp0\Lib\Win64
xcopy /Y /I %~dp0\interactive-cpp\Binaries\Release\x64\Interactivity.Win32.Cpp\*.* %~dp0\Lib\Win64
xcopy /Y /I %~dp0\interactive-cpp\Binaries\Release\x64\Casablanca\cpprestsdk140.uwp.static\*.* %~dp0\Lib\UWP64
xcopy /Y /I %~dp0\interactive-cpp\Binaries\Release\x64\Interactivity.UWP.Cpp\*.* %~dp0\Lib\UWP64
xcopy /Y /I %~dp0\interactive-cpp\Binaries\Release\Win32\Casablanca\cpprestsdk140.uwp.static\*.* %~dp0\Lib\UWP32
xcopy /Y /I %~dp0\interactive-cpp\Binaries\Release\Win32\Interactivity.UWP.Cpp\*.* %~dp0\Lib\UWP32

xcopy /Y /I %~dp0\interactive-cpp\Include\*.* %~dp0\Include\interactive-cpp
xcopy /Y /I /S %~dp0\interactive-cpp\cpprestsdk\release\include\*.* %~dp0\Include\interactive-cpp
xcopy /Y /I /S %~dp0\interactive-cpp-v2\source\*.* %~dp0\Include\interactive-cpp-v2