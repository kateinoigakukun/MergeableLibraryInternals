.PHONY: all clean

all: Build/Dynamic/Run Build/Static/Run Build/Mergeable/Run-Dynamic Build/Mergeable/Run-Static

clean:
	rm -rf Build

Build/Foo.o: Sources/Foo/Foo.c
	mkdir -p $(@D)
	xcrun clang -c $< -o $@
Build/main.o: Sources/Run/main.c
	mkdir -p $(@D)
	xcrun clang -c $< -o $@

Build/Dynamic/libFoo.dylib: Build/Foo.o
	mkdir -p $(@D)
	xcrun ld Build/Foo.o -dylib -o Build/Dynamic/libFoo.dylib -lSystem -syslibroot $(shell xcrun --show-sdk-path)
Build/Dynamic/Run: Build/main.o Build/Dynamic/libFoo.dylib
	mkdir -p $(@D)
	xcrun ld -LBuild/Dynamic -lFoo Build/main.o -o Build/Dynamic/Run -lSystem -syslibroot $(shell xcrun --show-sdk-path)

Build/Static/libFoo.a: Build/Foo.o
	mkdir -p $(@D)
	xcrun ar rcs Build/Static/libFoo.a Build/Foo.o
Build/Static/Run: Build/main.o Build/Static/libFoo.a
	mkdir -p $(@D)
	xcrun ld -LBuild/Static -lFoo Build/main.o -o Build/Static/Run -lSystem -syslibroot $(shell xcrun --show-sdk-path)

Build/Mergeable/libFoo.dylib: Build/Foo.o
	mkdir -p $(@D)
	xcrun ld Build/Foo.o -dylib -make_mergeable -o Build/Mergeable/libFoo.dylib -lSystem -syslibroot $(shell xcrun --show-sdk-path)
Build/Mergeable/Run-Dynamic: Build/main.o Build/Mergeable/libFoo.dylib
	mkdir -p $(@D)
	xcrun ld -LBuild/Mergeable -lFoo Build/main.o -o Build/Mergeable/Run-Dynamic -lSystem -syslibroot $(shell xcrun --show-sdk-path)
Build/Mergeable/Run-Static: Build/main.o Build/Mergeable/libFoo.dylib
	mkdir -p $(@D)
	xcrun ld -LBuild/Mergeable -merge-lFoo Build/main.o -o Build/Mergeable/Run-Static -lSystem -syslibroot $(shell xcrun --show-sdk-path)
