# Makefile for Screen Capture
# Requires MSBuild in PATH

SOLUTION = ScreenCapture.sln
CONFIG_DEBUG = Debug
CONFIG_RELEASE = Release
PLATFORM = x64

.PHONY: all clean debug release run

all: release

debug:
	@echo Building Debug configuration...
	msbuild $(SOLUTION) /p:Configuration=$(CONFIG_DEBUG) /p:Platform=$(PLATFORM) /m

release:
	@echo Building Release configuration...
	msbuild $(SOLUTION) /p:Configuration=$(CONFIG_RELEASE) /p:Platform=$(PLATFORM) /m

clean:
	@echo Cleaning build artifacts...
	msbuild $(SOLUTION) /t:Clean /p:Configuration=$(CONFIG_DEBUG) /p:Platform=$(PLATFORM)
	msbuild $(SOLUTION) /t:Clean /p:Configuration=$(CONFIG_RELEASE) /p:Platform=$(PLATFORM)
	@if exist build rmdir /s /q build

run: release
	@echo Running application...
	@build\Release\ScreenCapture.exe

help:
	@echo Available targets:
	@echo   all      - Build release (default)
	@echo   debug    - Build debug version
	@echo   release  - Build release version
	@echo   clean    - Clean build artifacts
	@echo   run      - Build and run release version
	@echo   help     - Show this help
