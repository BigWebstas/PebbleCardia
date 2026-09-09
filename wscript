#
# Cardia - Pebble app + background worker.
#
import os.path

top = '.'
out = 'build'

# Code shared by the foreground app and the background worker. Compiled into
# both; the worker copies are built with -DCARDIA_WORKER (see platform.h).
SHARED_SOURCES = [
    'src/c/analysis.c',
    'src/c/episodes.c',
    'src/c/settings.c',
    'src/c/engine.c',
]


def options(ctx):
    ctx.load('pebble_sdk')


def configure(ctx):
    ctx.load('pebble_sdk')


def build(ctx):
    ctx.load('pebble_sdk')

    build_worker = os.path.exists('worker_src')
    binaries = []

    cached_env = ctx.env
    for platform in ctx.env.TARGET_PLATFORMS:
        ctx.env = ctx.all_envs[platform]
        ctx.set_group(ctx.env.PLATFORM_NAME)

        app_elf = '{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        ctx.pbl_build(source=ctx.path.ant_glob('src/c/**/*.c'),
                      target=app_elf,
                      bin_type='app')

        if build_worker:
            worker_elf = '{}/pebble-worker.elf'.format(ctx.env.BUILD_DIR)
            worker_src = ctx.path.ant_glob('worker_src/c/**/*.c') + \
                [ctx.path.find_node(s) for s in SHARED_SOURCES]
            # The shared sources need <pebble_worker.h> and the smaller worker
            # ring buffers here (see platform.h / config.h). worker.c defines
            # CARDIA_WORKER itself so the entry point still builds under a
            # build service that regenerates this wscript and drops the define.
            ctx.pbl_build(source=worker_src,
                          target=worker_elf,
                          bin_type='worker',
                          defines=['CARDIA_WORKER'])
            binaries.append({'platform': platform,
                             'app_elf': app_elf,
                             'worker_elf': worker_elf})
        else:
            binaries.append({'platform': platform, 'app_elf': app_elf})
    ctx.env = cached_env

    ctx.set_group('bundle')
    ctx.pbl_bundle(binaries=binaries,
                   js=ctx.path.ant_glob(['src/pkjs/**/*.js',
                                         'src/pkjs/**/*.json',
                                         'src/common/**/*.js']),
                   js_entry_file='src/pkjs/index.js')
