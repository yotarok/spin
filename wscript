# -*- mode: python -*-

PACKAGE_NAME = ''
PACKAGE_VERSION = '0.0.1'
PACKAGE_DESCRIPTION = ''

UNITTEST_ENABLED = False

def get_platform():
    from subprocess import Popen, PIPE
    sys = Popen('uname', stdout=PIPE, shell=True).communicate()[0].strip()
    arch = Popen('uname -m', stdout=PIPE, shell=True).communicate()[0].strip()
    return (arch + b'-' + sys, sys, arch)

(PLATFORM, SYS, ARCH) = get_platform()

def options(opt):
    opt.load('compiler_cxx')
    # opt.load('unittest_gtest doxygen', tooldir='wafextra')
    if UNITTEST_ENABLED: opt.load('unittest_gtest')

    opt.add_option('--boost-incpath', action='store')
    opt.add_option('--sndfile-incpath', action='store')
    opt.add_option('--sndfile-libpath', action='store')
    opt.add_option('--yamlcpp-incpath', action='store')
    opt.add_option('--yamlcpp-libpath', action='store')
    opt.add_option('--gear-incpath', action='store')
    opt.add_option('--gear-libpath', action='store')
    opt.add_option('--openfst-incpath', action='store')
    opt.add_option('--openfst-libpath', action='store')
    opt.add_option('--opencl-incpath', action='store')
    opt.add_option('--opencl-libpath', action='store')


def configure(conf):
    for envname in ['debug', 'release']:
        conf.setenv(envname)
        conf.load('compiler_cxx')
        #conf.load('unittest_gtest doxygen', tooldir='wafextra')
        conf.check_cxx(lib='dl', uselib_store='DL')
        conf.check_cxx(lib='sndfile', 
                       includes=[conf.options.sndfile_incpath],
                       libpath=[conf.options.sndfile_libpath],
                       uselib_store='SNDFILE')

        conf.check_cc(fragment='''
#include <boost/version.hpp>
    int main() { return 0; }
        ''',
                      includes=[conf.options.boost_incpath],
                      msg="Checking boost headers",
                      uselib_store='BOOST_HEADERS')

        conf.check_cxx(lib='yaml-cpp',
                       includes=[conf.options.yamlcpp_incpath],
                       libpath=[conf.options.yamlcpp_libpath],
                       uselib_store='YAMLCPP')
        conf.check_cxx(lib='gear',
                       includes = [conf.options.gear_incpath],
                       libpath = [conf.options.gear_libpath],
                       use='SNDFILE YAMLCPP BOOST_HEADERS',
                       uselib_store = 'GEAR')
        conf.check_cxx(lib=['fst', 'fstscript'],
                       includes=[conf.options.openfst_incpath],
                       libpath=[conf.options.openfst_libpath],
                       use='DL',
                       uselib_store='OPENFST')

        conf.check_cxx(msg="Checking OpenFST compatiblity with 1.5.3",
                       cxxflags='-std=c++11',
                       fragment='''
#include <fst/script/fst-class.h>
#include <fst/script/map.h>
#include <fst/script/shortest-path.h>
#include <fst/script/rmepsilon.h>
using namespace fst::script;
int main() {
  VectorFstClass f1("standard"), f2("standard");

  // check Map
  FstClass* c = Map(f1, TO_STD_MAPPER, 0.0, WeightClass());

  // check RmEpsilon
  fst::script::RmEpsilon(&f1, true);

  // check ShortestPath
  std::vector<WeightClass> d;
  ShortestPathOptions opt(
    fst::AUTO_QUEUE, 1, false, true, 0.001,
    false, WeightClass("standard", ""), -1);
  ShortestPath(f1, &f2, &d, opt);

  return 0;
}
                       ''', use='OPENFST DL')


        if b'Darwin' in PLATFORM:
            ocl_found = conf.check_cxx(msg='Checking for OpenCL.framework',
                                       framework='OpenCL',
                                       uselib_store='OPENCL',
                                       mandatory=False)

        else:
            ocl_found = conf.check_cxx(lib='OpenCL', 
                       fragment='''
#include <CL/cl.h>
int main() { return 0; }
                       ''',
                                       includes=[conf.options.opencl_incpath, './3rd/nvidiaOCL'],
                                       libpath=[conf.options.opencl_libpath],
                                       rpath=[conf.options.opencl_libpath],
                                       mandatory=False,
                                       uselib_store='OPENCL')
        conf.env.OCL_FOUND = ocl_found

        if envname == 'debug':
            conf.env.CFLAGS = ['-g', '-DDEBUG', '-DENABLE_TRACE',
                               '-DVIENNACL_WITH_EIGEN', '-DVIENNACL_WITH_OPENCL']
            conf.env.CXXFLAGS = ['-g', '-DDEBUG', '-DENABLE_TRACE', '-std=c++11',
                                 '-DVIENNACL_WITH_EIGEN', '-DVIENNACL_WITH_OPENCL']
        else:
            # TODO: Currently, this cannot generate distributable package
            #   because the binaries are tuned to the native environment
            #   This behavior might be better to be optional.
            conf.env.CFLAGS = ['-O3', '-DNDEBUG',
                               '-march=native', '-mtune=native',
                               '-DVIENNACL_WITH_EIGEN', '-DVIENNACL_WITH_OPENCL']
            conf.env.CXXFLAGS = ['-O3', '-DNDEBUG',
                                 '-march=native', '-mtune=native',
                                 '-std=c++11',
                                 '-DVIENNACL_WITH_EIGEN', '-DVIENNACL_WITH_OPENCL']

        if ocl_found:
            conf.env.CFLAGS += ['-DVIENNACL_WITH_OPENCL', '-DSPIN_WITH_NNET']
            conf.env.CXXFLAGS += ['-DVIENNACL_WITH_OPENCL', '-DSPIN_WITH_NNET']


def import_text_files(task):
    import os.path
    dest = open(task.outputs[0].abspath(), 'w')
    print ('''
namespace spin {
    namespace resource {
''', file=dest)
    for clfile in task.inputs:
        varname = os.path.basename(clfile.abspath()).replace('.', '_')
        print('const char* {} ='.format(varname), file=dest)
        print('// ' + clfile.abspath(), file=dest)
        for l in open(clfile.abspath()):
            l = l.rstrip()
            print('"' + l.replace('\\', '\\\\').replace('"', '\\\"') + '\\n"', file=dest)
        print(';', file=dest)

    print('''
    }
}
''',file=dest)

def build(bld):
    if not bld.variant: 
        bld.fatal('specify variant')

    if bld.env.OCL_FOUND:
        bld(source = "src/resource/corpus_view.css src/resource/corpus_item_view.js",
            target = 'textres.cpp',
            rule = import_text_files,
            color = 'CYAN',
            name = 'Generating C++ source from text resources',
        )

    libsources = '''
textres.cpp
src/lib/corpus/yaml.cpp src/lib/corpus/msgpack.cpp  src/lib/corpus/corpus.cpp
src/lib/fscorer/diaggmm.cpp
src/lib/hmm/tree.cpp src/lib/hmm/treestat.cpp src/lib/io/fst.cpp
src/lib/io/file.cpp
src/lib/fst/linear.cpp src/lib/fst/text_compose.cpp src/lib/io/variant.cpp
'''
    if bld.env.OCL_FOUND:
        libsources += '''
src/lib/fscorer/nnet_scorer.cpp
src/lib/nnet/nnet.cpp src/lib/nnet/node.cpp src/lib/nnet/cache.cpp
src/lib/nnet/stream.cpp src/lib/nnet/signal.cpp src/lib/nnet/affine.cpp
src/lib/nnet/sigmoid.cpp src/lib/nnet/relu.cpp src/lib/nnet/random.cpp
src/lib/nnet/dropout.cpp src/lib/nnet/io.cpp
'''

    bld.stlib(features='cxx cxxstlib',
              source=libsources,
              target='spin',
              includes='include/ 3rd/ 3rd/msgpack',
              cxxflags=['-Wno-c++11-extensions'],
              use='YAMLCPP GEAR OPENFST SNDFILE OPENCL BOOST_HEADERS')

    progs = '''
corpus_copy corpus_list tree_to_hcfst corpus_fst_compose corpus_filter
equal_align gmm_acc gmm_init gmm_est corpus_view
align fst_compose decode corpus_fst_align gmm_split tree_acc tree_build_question
tree_split flow_feed corpus_fst_project tree_acc_merge gmm_acc_merge fst_trim
object_copy afftr_cmvn afftr_write_flow align_to_stid afftr_cmvn_acc
'''
    if bld.env.OCL_FOUND:
        prog += '''
nnet_empty nnet_add_node nnet_del_node nnet_shuffle_sgd nnet_eval
nnet_cancel_bias score_merge corpus_merge
bench_affine
'''
    #nnet_empty
    for prog in progs.strip().split():
        src = 'src/tool/' + prog + '.cpp'
        progname = 'spn_' + prog
        if prog.startswith("exp/"):
            progname = 'spn_' + prog[4:]

        bld.program(features = 'cxx cxxprogram',
                    source=src,
                    target=progname,
                    lib="clblas",
                    includes='include/ 3rd/ 3rd/msgpack',
                    cxxflags=['-Wno-c++11-extensions'],
                    use='spin YAMLCPP GEAR OPENFST SNDFILE OPENCL DL BOOST_HEADERS')

    ''''
    for subdir, test in [('io', 'msgpack'), ('io', 'yaml'), ('fscorer', 'diaggmm'),
                         ('hmm', 'tree'), ('utils', 'iterator'), ('utils', 'math'),
                         ('nnet', 'cache'), ('nnet', 'nnet'), ('nnet', 'random')]:
        #print('src/test/'+subdir+'/test_'+test+'.cpp')
        bld.program(features = 'cxx gtest',
                    source = 'src/test/'+subdir+'/test_'+test+'.cpp',
                    includes = 'include/ 3rd/ 3rd/msgpack',
                    target = 'spn_test_' + subdir.replace('/','_') + '_' + test,
                    defines = 'ENABLE_TRACE',
                    cxxflags=['-Wno-c++11-extensions'],
                    use = 'spin YAMLCPP GEAR OPENFST SNDFILE OPENCL DL')
    '''

    for dsoname in ['lattice-arc']:
        dso = bld.shlib(features = 'cxx cxxshlib',
                        source = 'src/fstext/' + dsoname + '.cpp',
                        includes='include/ 3rd/ 3rd/msgpack',
                        target=dsoname,
                        install_path="${PREFIX}/lib",
                        cxxflags=['-Wno-c++11-extensions', '-fPIC'],
                        use='OPENFST DL BOOST_HEADERS')
        dso.env.cxxshlib_PATTERN = '%s.so'

    bld.install_files('${PREFIX}',
                      bld.path.ant_glob('include/spin/**/*.hpp'),
                      cwd=bld.path, relative_trick=True)

    if bld.env.DOXYGEN:
        bld(features="doxygen", doxyfile="Doxyfile")



from waflib.Build import BuildContext, CleanContext, \
        InstallContext, UninstallContext

for x in 'debug release'.split():
    for y in (BuildContext, CleanContext, InstallContext, UninstallContext):
        name = y.__name__.replace('Context','').lower()
        class tmp(y):
            cmd = name + '_' + x
            variant = x
