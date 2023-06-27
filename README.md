# Overview mechanism of Mergeable Library

Please note that this document is unofficial and may be changed in the future.


## Introduction

Mergeable Library is a linkable library format for Mach-O. It is designed to be able to defer the decision of link-strategy, dynamic or static linking, until the final link step.

The traditional linking way requires to decide its link-strategy at creating a library.

- A dynamically linkable library (`.dylib`) is created by the linker (`ld`) with `-dylib` option. It doesn't have relocation info.
- A statically linkable library (`.a`) is created by the archiver (`ar`). It keeps the input relocation info.

```
# Link dynamic library Foo

$ clang -c Foo.c -o Foo.o
$ ld Foo.o -dylib -o libFoo.dylib -lSystem -syslibroot $(xcrun --show-sdk-path)
```

```
# Link static library Foo

$ clang -c Foo.c -o Foo.o
$ ar rcs libFoo.a Foo.o
```

In addition, those two kinds of library format have difference in their image file identity. A dynamic library has its own file at runtime, but a static library doesn't.
`NSBundle` depends on the image file identity to find its framework resouces, so a static library cannot have its framework resource at runtime. To reference resources from a static library code,
it has to reference other frameworks or bundles.
So, it's hard to keep the same project structure between static and dynamic linking.

Mergeable Library is designed to solve those problems. It is a linkable library format for Mach-O, and it can be linked as both dynamic and static library.
It is also designed to have its own framework resources as if it is a dynamic library even if it's linked statically.

## Mergeable Library

Mergeable Library file can be considered as a special form of dynamic library. The difference from a dynamic library is that Mergeable Library has relocation info, which is used to link it statically. (It's stored in `LC_ATOM_INFO`'s payload)

A Mergeable Library can be created by the following commands:

```
$ clang -c Foo.c -o Foo.o
$ ld Foo.o -dylib -make_mergeable -o libFoo.dylib -lSystem -syslibroot $(xcrun --show-sdk-path)
$ ld -L. -lFoo main.o -o Run-Dynamic -lSystem -syslibroot $(xcrun --show-sdk-path)
$ ld -L. -merge-lFoo main.o -o Run-Static -lSystem -syslibroot $(xcrun --show-sdk-path)
```

Please note that the link step uses `-make_mergeable` option to add relocation info to the output mergeable library. `-l` is used to link the mergeable library dynamically and `-merge-l` is used to link statically.

A Mergeable Library can be embeded in a framework and it can be dynamically linked with `-framework` linker option or statically linked with `-merge_framework` option. The merged framework can be embedded in the app bundle and the framework has an empty library binary.

## `LC_ATOM_INFO` segment

As mentioned in the last section, `LC_ATOM_INFO` load command's payload contains relocation info to statically link the library into the final image file. The load command type is added in Xcode 15 and you can see its definition in `mach-o/loader.h` in each platform SDK shipped with Xcode. The segment payload of the `LC_ATOM_INFO` is still unknown but it looks like it's not the same format as the usual relocation info and as the name implies it may be a serialized format of the internal Darwin linker's representation [Atom](https://opensource.apple.com/source/ld64/ld64-136/doc/design/linker.html#:~:text=non%2Dlive%20atoms.-,Atom%20model). Atom holds the enough information to reproduce relocation info.

```c
#define LC_ATOM_INFO 0x36 /* used with linkedit_data_command */

/*
 * The linkedit_data_command contains the offsets and sizes of a blob
 * of data in the __LINKEDIT segment.
 */
struct linkedit_data_command {
    uint32_t    cmd;            /* LC_CODE_SIGNATURE, LC_SEGMENT_SPLIT_INFO,
                                   LC_FUNCTION_STARTS, LC_DATA_IN_CODE,
                                   LC_DYLIB_CODE_SIGN_DRS, LC_ATOM_INFO,
                                   LC_LINKER_OPTIMIZATION_HINT,
                                   LC_DYLD_EXPORTS_TRIE, or
                                   LC_DYLD_CHAINED_FIXUPS. */
    uint32_t    cmdsize;        /* sizeof(struct linkedit_data_command) */
    uint32_t    dataoff;        /* file offset of data in __LINKEDIT segment */
    uint32_t    datasize;       /* file size of data in __LINKEDIT segment  */
};
```

## `Bundle(for: AnyClass)` hook for statically linked Mergeable Library

Foundation's `Bundle(for: AnyClass)` finds a bundle path which contains the given class code. It is usually used to reference framework-local resources.
It internally uses `class_getImageName` [^1] [^2] to get the image file path of the given class. `class_getImageName` returns the dynamic library name based on the given class code address.
By default, `class_getImageName` uses `dyld_image_path_containing_address` to find the image file path containing the given class code. [^3]

```objc
/** 
 * Returns the dynamic library name a class originated from.
 * 
 * @param cls The class you are inquiring about.
 * 
 * @return The name of the library containing this class.
 */
OBJC_EXPORT const char * _Nullable
class_getImageName(Class _Nullable cls)
```

However, a statically linked library doesn't have its own image file, so `dyld_image_path_containing_address` returns the linked image file path instead of the static library path.
This difference makes `Bundle(for: AnyClass)` return a inconsistent path based on how the mergeable library is linked statically or dymamically.

To solve this problem, the Darwin linker synthesizes a piece of code and data into the linked image when linking mergeable framework statically (`-merge_framework`). The linker synthesized code is invoked from [static constructor](https://gcc.gnu.org/onlinedocs/gccint/Initialization.html) and it installs a hook for `class_getImageName` by [`objc_setHook_getImageName`](https://github.com/apple-oss-distributions/objc4/blob/689525d556eb3dee1ffb700423bccf5ecc501dbf/runtime/runtime.h#L1713-L1732).

```objc
/**
 * Install a hook for class_getImageName().
 *
 * @param newValue The hook function to install.
 * @param outOldValue The address of a function pointer variable. On return,
 *  the old hook function is stored in the variable.
 *
 * @note The store to *outOldValue is thread-safe: the variable will be
 *  updated before class_getImageName() calls your new hook to read it,
 *  even if your new hook is called from another thread before this
 *  setter completes.
 * @note The first hook in the chain is the native implementation of
 *  class_getImageName(). Your hook should call the previous hook for
 *  classes that you do not recognize.
 *
 * @see class_getImageName
 * @see objc_hook_getImageName
 */
OBJC_EXPORT void objc_setHook_getImageName(objc_hook_getImageName _Nonnull newValue,
                                           objc_hook_getImageName _Nullable * _Nonnull outOldValue)
```

The linker synthesized static constructor code is below. It installs `__ZL13imageNameHookP10objc_classPPKc` (`imageNameHook(objc_class*, char const**)`[^4]) as a hook, which is also generated by the linker.

Those code sequence are directly embedded in `ld-prime` and they can be found as `BundleForClassHook_macos_arm64`, `BundleForClassHook_macos_arm64`, and so on for each platforms. (You can see the code content that begins with `cf fa ed fe`, which is the magic bytes of Mach-O binary in `ld-prime` binary)

```
(__TEXT,__text) section
__ZL11constructorv:
100004000:      fd 7b bf a9     stp     x29, x30, [sp, #-16]!
100004004:      fd 03 00 91     mov     x29, sp
100004008:      00 00 00 b0     adrp    x0, 1 ; 0x100005000
10000400c:      00 c0 2d 91     add     x0, x0, #2928 ; literal pool for: "s14PartialKeyPathCyytG"
100004010:      c1 02 80 52     mov     w1, #22
100004014:      02 00 80 d2     mov     x2, #0
100004018:      03 00 80 d2     mov     x3, #0
10000401c:      18 05 00 94     bl      0x10000547c ; symbol stub for: _swift_getTypeByMangledNameInContext
100004020:      00 00 00 90     adrp    x0, 0 ; 0x100004000
100004024:      00 e0 00 91     add     x0, x0, #56 ; __ZL13imageNameHookP10objc_classPPKc
100004028:      41 00 00 90     adrp    x1, 8 ; 0x10000c000
10000402c:      21 00 1d 91     add     x1, x1, #1856
100004030:      fd 7b c1 a8     ldp     x29, x30, [sp], #16
100004034:      fa 04 00 14     b       0x10000541c ; symbol stub for: _objc_setHook_getImageName
```

The `imageNameHook` function is called when `class_getImageName` is called. It returns the merged framework path whose code is statically linked but resources are held in the embedded framework. Here is the pseudo code of `imageNameHook`.

```objc++
struct FrameworkLocation {
  const char *name;
  void *unknown;
};

struct LibraryClass {
  void *isa;
  FrameworkLocation *location;
};

static FrameworkLocation relinkableLibraryFrameworkLocations[] = {
  { "MyUI", NULL },
  { "MyUI", NULL },
  { "MyUI", NULL },
  { "MyUI", NULL },
};

static LibraryClass relinkableLibraryClasses[] = {
  { (void *)&_OBJC_CLASS_$_Foo, &relinkableLibraryFrameworkLocations[0] },
  { (void *)&_OBJC_CLASS_$_Bar, &relinkableLibraryFrameworkLocations[1] },
  { (void *)&_OBJC_CLASS_$_Baz, &relinkableLibraryFrameworkLocations[2] },
  { (void *)&_OBJC_CLASS_$_Qux, &relinkableLibraryFrameworkLocations[3] },
};
static const size_t relinkableLibraryClassesCount = 4;

static std::unordered_map<void *, const char *> classes;
void makeClassMap(void) {
  for (size_t i = 0; i < relinkableLibraryClassesCount; i++) {
    classes.try_emplace(relinkableLibraryClasses[i].isa,
                        relinkableLibraryClasses[i].location->name);
  }
}

const char *imageNameHook(Class cls, const char **outOldValue) {
  if (classes.empty()) {
    makeClassMap();
  }
  auto it = classes.find(cls);
  if (it == classes.end()) {
    return original_imageNameHook(cls, outOldValue);
  }
  const char *name = it->second;
  NSString *path = [NSString stringWithFormat:@"Frameworks/%s.framework/%s", name, name];
  NSBundle *bundle = [NSBundle mainBundle];
  NSURL *url = [NSURL URLWithString:path relativeToURL:[bundle bundleURL]];
  return strdup([[url path] fileSystemRepresentation]);
}
```

The `relinkableLibraryFrameworkLocations`, `relinkableLibraryClasses`, and `relinkableLibraryClassesCount` data are generated at link-time. The memory layout of them was inspected using debugger.

[^1]: https://github.com/apple-oss-distributions/objc4/blob/689525d556eb3dee1ffb700423bccf5ecc501dbf/runtime/runtime.h#L1454-L1462
[^2]: https://developer.apple.com/documentation/objectivec/1418539-class_getimagename
[^3]: https://github.com/apple-oss-distributions/objc4/blob/689525d556eb3dee1ffb700423bccf5ecc501dbf/runtime/objc-runtime.mm#L584-L589
[^4]: Demangled by `$ llvm-cxxfilt __ZL13imageNameHookP10objc_classPPKc`
