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

const SRC = [_][]const u8{ "drw.c", "dwm.c", "util.c" };

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = .ReleaseSmall;
    const bin = b.addExecutable(.{
        .name = "dwm",
        .target = target,
        .optimize = optimize,
    });
    bin.addCSourceFiles(&SRC, &CFLAGS);

    bin.addAnonymousModule("config.h", .{ .source_file = .{ .path = "config.h" } });
    bin.addLibraryPath(X11LIB);
    for (INCS) |path| {
        bin.addIncludePath(path);
    }
    for (LIBS) |lib| {
        bin.linkSystemLibrary(lib);
    }
    const install_step = b.addInstallFileWithDir(
        bin.getEmittedBin(),
        .{ .custom = b.pathJoin(&.{ PREFIX, "bin" }) },
        "dwm",
    );
    const install_man = b.addInstallFileWithDir(
        .{ .path = "dwm.1" },
        .{ .custom = b.pathJoin(&.{ MANPREFIX, "man1" }) },
        "dwm.1",
    );
    const install_desktop = b.addInstallFileWithDir(
        .{ .path = "dwm.desktop" },
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
}
