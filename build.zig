const std = @import("std");

const VERSION = "6.4";
const PREFIX = "usr/local";
const MANPREFIX = PREFIX ++ "/share/man";

const X11INC = .{ .path = "/usr/include/xorg" };
const X11LIB = .{ .path = "/usr/lib/xorg" };

const FREETYPELIBS = [_][]const u8{ "fontconfig", "Xft" };
const FREETYPEINC = .{ .path = "/usr/include/freetype2" };

const INCS = [_]std.Build.LazyPath{ X11INC, FREETYPEINC };
const LIBS = [_][]const u8{"X11"} ++ FREETYPELIBS;

const CPPFLAGS = [_][]const u8{
    "-D_DEFAULT_SOURCE",
    "-D_BSD_SOURCE",
    "-D_XOPEN_SOURCE=700L",
    "-DVERSION=\"" ++ VERSION ++ "\"",
};
const CFLAGS = [_][]const u8{
    "-std=c99",
    "-pedantic",
    "-Wall",
    "-Wno-depricated-declarations",
} ++ CPPFLAGS;
const LDFLAG = LIBS;

const XEPHYR_CMD = [_][]const u8{
    "Xephyr",
    "-br",
    "-ac",
    "-noreset",
    "-screen",
    "800x600",
    ":1",
};
const SRC = [_][]const u8{ "src/drw.c", "src/dwm.c", "src/util.c" };

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = .ReleaseSmall;
    const drw_mod = b.addObject(.{
        .name = "drw",
        .root_source_file = .{ .path = "src/drw.zig" },
        .optimize = optimize,
        .target = target,
        .link_libc = true,
    });

    const bin = b.addExecutable(.{
        .name = "dwm",
        .root_source_file = .{ .path = "src/main.c" },
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    bin.addObject(drw_mod);
    bin.addCSourceFiles(&SRC, &CFLAGS);
    bin.addIncludePath(.{ .path = b.pathFromRoot("") });
    bin.addLibraryPath(X11LIB);
    for (INCS) |path| {
        drw_mod.addIncludePath(path);
        bin.addIncludePath(path);
    }
    for (LIBS) |lib| {
        drw_mod.linkSystemLibrary(lib);
        bin.linkSystemLibrary(lib);
    }
    const install_step = b.addInstallFileWithDir(
        bin.getEmittedBin(),
        .{ .custom = b.pathJoin(&.{ PREFIX, "bin" }) },
        "dwm",
    );
    const install_man = b.addInstallFileWithDir(
        .{ .path = "assets/dwm.1" },
        .{ .custom = b.pathJoin(&.{ MANPREFIX, "man1" }) },
        "dwm.1",
    );
    const install_desktop = b.addInstallFileWithDir(
        .{ .path = "assets/dwm.desktop" },
        .{ .custom = b.pathJoin(&.{"usr/share/xsessions"}) },
        "dwm.desktop",
    );
    for ([_]*std.Build.Step.InstallFile{
        install_step,
        install_man,
        install_desktop,
    }) |file| {
        b.getInstallStep().dependOn(&file.step);
    }

    const clean_step = b.step("clean", "clean build dirctories.");
    for ([_][]const u8{
        "zig-out",
        "zig-cache",
    }) |dir| {
        const rm_cmd = b.addRemoveDirTree(dir);
        clean_step.dependOn(&rm_cmd.step);
    }

    //    const run_xephyr_cmd = b.addSystemCommand(&XEPHYR_CMD);

    const run_wm_cmd = b.addRunArtifact(bin);

    run_wm_cmd.setEnvironmentVariable("DISPLAY", ":1");
    const test_step = b.step("test", "test the application");
    test_step.dependOn(&run_wm_cmd.step);
}
