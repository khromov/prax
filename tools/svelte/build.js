// Bundles a Svelte application for the engine's QuickJS: `bun build.js [--backend dom|kit] <entry.js> <out-directory>`.
//
// The entry is a module that imports its components and mounts one with `mount` from 'prax-svelte'. Each component is
// compiled for the custom renderer of the backend, and the bundle holds the application, the renderer and Svelte's
// client runtime as one file, named for the entry, which is all that the engine evaluates.
//
// The dom backend's bundle is an ES module, and beside it goes a page of the same name that loads it. The kit
// backend's bundle is a classic script, which its host runs in a realm with no document of its own to load modules
// from (tools/svelte/kit_host.cpp).

import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { basename, dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));

// Where Svelte and what it depends on are installed (tools/svelte/bundle.py), outside the sources.
const INSTALLED = join(HERE, '..', '..', 'third_party', 'svelte');
if (!existsSync(join(INSTALLED, 'node_modules', 'svelte', 'package.json'))) {
    console.error('build.js: no Svelte is installed under %s, which ./dev.sh svelte installs', INSTALLED);
    process.exit(1);
}
const { compile, compileModule } = await import(Bun.resolveSync('svelte/compiler', INSTALLED));

// The module of each backend, which is both the renderer that compiled components import and what an application
// imports as 'prax-svelte'.
const BACKENDS = { dom: join(HERE, 'src', 'dom.js'), kit: join(HERE, 'src', 'kit.js') };

// The conditions that pick Svelte's client runtime for a custom renderer, without its development checks.
const CONDITIONS = ['custom-renderer', 'production'];

// Returns the file that an entry of a package's exports names under the accepted conditions.
function pick(target, accepted) {
    if (typeof target !== 'object')
        return target;
    for (const [condition, inner] of Object.entries(target)) {
        const found = accepted.has(condition) ? pick(inner, accepted) : undefined;
        if (found)
            return found;
    }
    return undefined;
}

// Returns a resolver that answers every import of Svelte with the vendored copy, wherever the importer is. An
// application's own directory would otherwise resolve none, or another build that lacks the renderer API.
function resolveSvelte() {
    const directory = join(INSTALLED, 'node_modules', 'svelte');
    const manifest = JSON.parse(readFileSync(join(directory, 'package.json'), 'utf8'));
    if (!manifest.exports['./renderer'])
        throw new Error('the Svelte under ' + INSTALLED + ' has no custom renderer API');
    const accepted = new Set([...CONDITIONS, 'browser', 'import', 'default']);
    return (args) => {
        const key = args.path === 'svelte' ? '.' : './' + args.path.slice('svelte/'.length);
        const target = pick(manifest.exports[key], accepted);
        return target ? { path: join(directory, target) } : undefined;
    };
}

// Compiles a component for the renderer at `renderer`. The compiler scopes the component's CSS and returns it as
// text, which the compiled module hands to the renderer as it is evaluated.
function compileComponent(path, renderer, backend) {
    let scope = null;
    const result = compile(readFileSync(path, 'utf8'), {
        filename: path,
        generate: 'client',
        runes: true,
        css: 'external',
        cssHash: ({ hash, css }) => (scope = 'svelte-' + hash(css)),
        discloseVersion: false,
        // The compiler's accessibility checks read the kit's elements as HTML's of the same names.
        warningFilter: (warning) => backend === 'dom' || !warning.code.startsWith('a11y'),
        // A boundary's pending and failed snippets do not compile for a custom renderer without async.
        experimental: { customRenderer: renderer, async: true },
    });
    for (const warning of result.warnings)
        console.warn('%s:%d: %s', path, warning.start?.line ?? 0, warning.message);
    let code = result.js.code;
    if (scope !== null && result.css?.code) {
        code += '\nimport { defineStyles as $$defineStyles } from ' + JSON.stringify(renderer) + ';\n';
        code += '$$defineStyles(' + JSON.stringify(scope) + ', ' + JSON.stringify(result.css.code) + ');\n';
    }
    return code;
}

// Compiles a module that uses runes outside a component, a `.svelte.js` or `.svelte.ts` file.
function compileRunes(path) {
    let source = readFileSync(path, 'utf8');
    if (path.endsWith('.ts'))
        source = new Bun.Transpiler({ loader: 'ts' }).transformSync(source);
    return compileModule(source, { filename: path, generate: 'client' }).js.code;
}

async function main(argv) {
    let backend = 'dom';
    const rest = [];
    for (let i = 0; i < argv.length; ++i) {
        if (argv[i] === '--backend')
            backend = argv[++i];
        else
            rest.push(argv[i]);
    }
    if (rest.length !== 2 || !(backend in BACKENDS)) {
        console.error('usage: bun build.js [--backend %s] <entry.js> <out-directory>', Object.keys(BACKENDS).join('|'));
        return 2;
    }
    const entry = resolve(rest[0]);
    const out = resolve(rest[1]);
    if (!existsSync(entry)) {
        console.error('build.js: no entry at %s', entry);
        return 1;
    }
    const renderer = BACKENDS[backend];
    const result = await Bun.build({
        entrypoints: [entry],
        outdir: out,
        target: 'browser',
        format: backend === 'kit' ? 'iife' : 'esm',
        conditions: CONDITIONS,
        define: { 'process.env.NODE_ENV': '"production"' },
        throw: false,
        plugins: [{
            name: 'prax-svelte',
            setup(build) {
                build.onResolve({ filter: /^prax-svelte$/ }, () => ({ path: renderer }));
                build.onResolve({ filter: /^svelte(\/|$)/ }, resolveSvelte());
                build.onLoad({ filter: /\.svelte\.[jt]s$/ }, (args) => ({
                    contents: compileRunes(args.path),
                    loader: 'js',
                }));
                build.onLoad({ filter: /\.svelte$/ }, (args) => ({
                    contents: compileComponent(args.path, renderer, backend),
                    loader: 'js',
                }));
            },
        }],
    });
    if (!result.success) {
        for (const message of result.logs)
            console.error(String(message));
        return 1;
    }
    if (backend !== 'dom')
        return 0;
    const name = basename(entry).replace(/\.[^.]+$/, '');
    mkdirSync(out, { recursive: true });
    writeFileSync(join(out, name + '.html'), [
        '<!DOCTYPE html>',
        '<html>',
        '<head>',
        '<meta charset="UTF-8" />',
        '<title>' + name + '</title>',
        '</head>',
        '<body>',
        '<script type="module" src="' + name + '.js"></script>',
        '</body>',
        '</html>',
        '',
    ].join('\n'));
    return 0;
}

process.exit(await main(process.argv.slice(2)));
