# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Prax is a set of static C libraries, implemented in C++20, for GUI applications and games: platform abstractions,
implementations of web and other specifications (CSS, DOM, HTML, SVG, MathML, Lottie, Markdown, TeX, fetch, …), and
opinionated layers on top (immediate-mode UI, Studio Kit). It targets macOS 26+ on Apple Silicon, Windows 10 1809+, iOS
and the web (Emscripten). Development needs CMake 4.0+, Ninja and Python 3.

## Commands

Everything goes through `./dev.sh` (a Python package, `tools/`, dispatched by `tools/__main__.py`). `./dev.sh help`
lists every command, and `./dev.sh <command> --help` describes one.

```sh
./dev.sh test                               # the fast loop: lint + build + every unit test + the tools' Python tests
./dev.sh test dom                           # one module's doctest binary (builds only that binary)
./dev.sh test dom -ts="*keyword*" --success # doctest options pass through to that binary
./dev.sh test -tc="*flood*"                 # doctest options with no module: run matching cases in every binary
./dev.sh test debug dom                     # the same against the debug build, which is the one that checks PR_ASSERT
python3 -m unittest tools.lint.test_lint    # one Python tool test, against the default build's harnesses
./dev.sh ci                                 # lint, then default + debug builds and tests on every platform it can build
./dev.sh build [debug|release|asan|tsan] [wasm|ios|ios-sim] [--target NAME]
./dev.sh suite <name> [--match <glob>]      # a conformance suite; --ab compares against a base commit
./dev.sh lint                               # the conventions the compiler cannot check (tools/lint/lint.py)
./dev.sh layout | includes | rewrap [path…] # auto-fix code layout, include order, comment wrapping
./dev.sh prose                              # check a comments-only change (code must be unchanged)
./dev.sh gen [name…]                        # regenerate generated tables (never hand-edit generated files)
./dev.sh package                            # build/install as a consumer would and build tools/package against it
./dev.sh example <name> / app <name>        # build and run something under examples/ or apps/
./dev.sh bench ab [name…] [--base REF]      # measure a change against a base, in retired instructions
./dev.sh spec <name> [pattern]              # read a spec's editor's draft verbatim
./dev.sh probe <page> [--screenshot DIR]    # measure/screenshot a page in Chrome, WebKit, Firefox and the engine
```

Build directories are `build/` (default, RelWithDebInfo), `build-debug/`, `build-asan/`, `build-wasm/`, etc., with a
`-lean` variant used by `ci`. `PR_BUILD_DIR` renames the default build's directory, for a second checkout or a build
that must not be shared with another running at the same time. Nothing under `third_party/` is committed: the build
fetches pinned, hash-checked dependencies (`cmake/dependencies.cmake`, `cmake/test_data.cmake`) and commands fetch the
corpora they need (`tools/fetch/checkouts.py`). The test runner writes each failure's full output to `tests.log` in the
build directory. `PR_TEST_NO_GPU=1` skips the tests that need a GPU, as CI does.

## Architecture

The libraries are layered so an application takes only the parts it needs. For text, `lib/font` gives glyph paths
and metrics, `lib/draw` rasterizes, `lib/text` shapes and breaks lines, `lib/layout` runs the CSS layout algorithms, and
`lib/ui` adds immediate-mode interaction on top. The README's table groups every module by area, and the first comment
of each module's `CMakeLists.txt` says what it holds. The layering:

- **C API first.** Public headers in `lib/<module>/include/pr/` are C, included as `"pr/<name>.h"`. The
  implementation in `lib/<module>/src` is C++ in namespace `pr_impl`, written with core's toolkit (`lib/core/cpp`). A
  language layer is a sibling library in `lib/<module>/<layer>/` (`cpp`, `quickjs`, `quickjs_cpp`), and only `cpp`
  layers have C++ public headers (namespace `pr`). Applications under `examples/`, `apps/` and `tools/` use `pr` and
  never name `pr_impl`.
- **Targets.** `pr_library(<name>)` (`cmake/build.cmake`) creates `pr_<name>`, which every target links as
  `prax::<name>`. A target links exactly the modules whose headers it includes, publicly if a public header includes
  them (lint's `module-dependency` check). A library includes another library's public headers only. Its own tests,
  bindings and backends are the only code that may include its `src/` headers. CSS, DOM, HTML, SVG, MathML and XHR
  share their implementations for now.
- **Interfaces and backends.** An interface module (app, gpu, draw renderers, fetch, net, tls, text shaper, …)
  defines structs of function pointers in `pr/<tag>_backend.h`. Its backends live in `lib/<module>/backends/<backend>/`
  and are compiled into the module's library when `PR_<MODULE>_<BACKEND>` is on. A program picks a backend at run time
  through `Pr<Interface>Backend` and `pr<Interface>Create`, or brings its own. `comparisons/` wrap other implementations
  (Blend2D, Yoga, …) for benchmarking and are not product backends. Dev builds (`PR_DEVELOP`, `PR_EXAMPLES`,
  `PR_APPS`) turn on every backend the platform supports.
- **Platform.** Everything the library does to the host on its own initiative goes through core's C API: memory,
  failure, threads, files, clocks, vmem, environment, random. It is bound at compile time, and `PR_USER_CONFIG` lets an
  embedder rebind whole groups (`docs/platform.md`). Only the core source that wraps a standard or system header may
  include it.
- **Rendering pipeline.** The page's thread parses, runs QuickJS, owns the DOM and computes styles. A layout thread
  per agent lays out and records scenes, and a compositor plays them. Every stage caches its results and recomputes
  only what a change reaches. `prCssPresentationVerify` and `prLayoutTreeVerify` check the cache against a full
  recomputation, and `pr_shell --verify` and the suites' `--verify` run those checks on real pages
  (`docs/rendering.md`).
- **Two clients, one engine.** The web (DOM, cascade) and the immediate-mode UI (`lib/ui`, which declares a layout
  tree each frame) share layout, text and drawing. `lib/layout` knows nothing of the DOM. Studio Kit
  (`lib/studio/kit`) is a component library over `lib/ui`.
- **Tools.** `tools/<concept>/` holds one dev.sh command's scripts, C++ harnesses and `commands.py`. `tools/shell`
  builds `pr_shell`, the headless page host that the suites, benchmarks and `document` drive. `tools/harness` holds
  code shared by harnesses and tests.
- **Apps and examples.** `apps/` (browser, scribe, merge, terminal, alchemy) are daily-use apps, each keeping its
  model in a library its unit tests link. `examples/` demonstrate a library. `examples/draw_gallery` is the smallest
  consumer.

## Tests and suites

Each module has one doctest binary, `lib/<module>/tests` (`pr_module_tests`), plus a C translation unit that compiles
the public headers. Tests are held to the same no-standard-headers rule as the libraries, and each writes its files
in `harness::testFolder()`. Python tool tests are `tools/**/test_*.py`.

Conformance suites keep their data in `suites/<suite>/` and their runners in `tools/suite/`. After a change, run the
suites for the areas it touches. `docs/testing.md` has the table: for example, CSS → `css-parsing`, `css-wpt`;
HTML → `html-tokens`, `html-tree`, `html-wpt`; Markdown → `commonmark`, `gfm`, `markdown-dom`; invalidation and
caching → the `*-wpt` suites with `--verify`; public headers or links → `./dev.sh package`. Commit
`suites/*/scoreboard.md` with the change that alters it. `errata.txt` is only for cases where the engine is right
and the corpus is wrong, never to excuse a missing feature. Unimplemented cases go in `deferred.txt`, cut features in
`cut.txt`. A run fails on an unrecorded failure, or when a recorded case starts passing.

When a spec, the browsers and the tests disagree, decide as `docs/behavior.md` describes. Read the spec verbatim
(`./dev.sh spec`), measure the browsers (`./dev.sh probe`), and read the issues. Generally follow the implementations
if they agree, and lean toward the spec if they disagree. Present a decision that departs from those rules to the
developer with a recommendation.

Performance is measured in retired instructions with `./dev.sh bench ab`. Power counts more than wall time: going wide
on threads is only for work that someone is waiting on (`docs/performance.md`).

## Conventions

`docs/conventions.md` is the authority, and `./dev.sh lint` enforces most of it. Read it before writing code. Its
reference module at the end shows the expected shape of a header and a source. The rules most often missed:

- **Names.** `Pr<Tag><Name>` types, `pr<Tag><Subject><Verb>` functions, `Pr<Type>_<Name>` enum values, `k`-prefixed
  constants. Every public name starts with a tag that one module owns (lint lists them). Implementation names in
  `pr_impl` start with a module word (`layoutBlock`, `CssValue`). Getters have no `Get`. Lookups are `From<Source>`.
  No `self` parameters. `Desc` holds required inputs, `Options` (with a `Default` function) optional ones, `Info`
  reported ones.
- **C++ restrictions in implementations.** No `new`, `delete` or `<new>`, and no constructors or destructors. Use
  `alloc<T>`, `allocZeroed<T>`, `RawArray` and `PrRawString`. No exceptions, no `auto` except where no type can be
  named, and no standard-library or system headers outside the core source that wraps them. Examples, tools and tests
  may use core's managed layer (`Array`, `String`, `Map`, …).
- **API shapes.** Strings are `PrStringView` in and are never NUL-terminated. Closed sets are enums, never `bool`
  parameters. A call fails with `bool` or `PrStatus` plus out parameters. Lists are count + index, count + pointer, or
  `out`/`capacity` that returns the total. External data is validated and returns a status, while caller contracts are
  `PR_ASSERT`. `Create` pairs with `Destroy` or `Release`, and both accept null.
- **Files.** Every file basename is unique across `lib`. A source has the same basename as its header and defines
  functions in the header's order. Generated files carry a banner: change the generator or its pinned input, then run
  `./dev.sh gen`.
- **Formatting.** 4-space indent, a 120-column limit, and LF line endings. A continuation is indented one level past
  the line it continues and never aligned to a column. A function or control head that wraps continues two levels in.
  A ternary fits on one line or becomes statements. `./dev.sh layout` fixes most of this.
- **Comments are technical writing** (`docs/technical_writing.md`). Say what the name and type don't: invariants,
  ownership, threading and cost, each in its own sentence. Don't narrate the implementation or its history, and don't
  write fragments that lean on the name. A comment on its own line covers everything down to the next blank line.
  Cite specifications as `css-display-4/#the-display-properties`, never other implementations. Code never links to
  `docs/`.

Subsystem design docs live in `docs/` (`css.md`, `dom.md`, `html.md`, `layout.md`, `draw.md`, `gpu.md`, `text.md`,
`svg.md`, `ui.md`, `component_library.md`, …). Read the one for the area you are changing.
