#!/usr/bin/env python
import os
import sys
import copy
import glob
import time
import select
import shutil
import itertools
import multiprocessing
from ConfigParser import ConfigParser

import logging
logger = multiprocessing.log_to_stderr()
logger.setLevel(multiprocessing.SUBWARNING)

from lib.tarantool_server import TarantoolServer, LuaTest
from lib.server import Server

threads = 2

#class MPPool(object):
#    def __init__(self, names, queue):
#        self.number = len(names)
#        self.unused = self.number
#        self.names = names
#        self.Queue = multiprocess.Queue()
#        self.running = {}
#
#    def map(self, cmd, *iterables):
#        self.cmd = cmd
#        self.iterables = iterables
#        self.iter = repeat(itertools.izip(itertools.repeat(self.Queue), self.names, *self.iterables))
#        self.iter_names =
#
#
#    def next(name):
#        return multiprocessing.Process(target=cmd, args=self.iterator.next())

class MockArgs(object):
    test = [""]
    suite = []
    suites = []
    is_force = False
    start_and_exit = False
    gdb = False
    valgrind = False
    builddir = ".."
    vardir = "varpal"
    mem = False

def search_suits(server):
    suites = []
    for root, dirs, names in os.walk(os.getcwd()):
        if 'suite.ini' in names:
            suites.append(os.path.basename(root))

    suites = [TestSuite(suite, server) for suite in sorted(suites)]
    suites = [x for x in suites if 'parallel' in x.ini and x.ini['parallel']]
    print suites
    return suites

def run_tests(suites, tnt_serv):
    global threads
    from collections import deque
    run_suits = deque()
    run_suits.extend(suites)
    globio = multiprocessing.Queue()
    running = {}

    proc_names = deque()
    proc_names.extend(["Process-" + str(i) for i in xrange(threads)])
    print run_suits
    print proc_names
    def run_processes():
        while len(proc_names):
            if len(run_suits) == 0:
                break
            suite = run_suits.pop()
            proc_name = proc_names.pop()
            print "::::: created", proc_name
            proc = multiprocessing.Process(name=proc_name, target=TestSuite.run_all, args=(suite, tnt_serv, globio, proc_name))
            proc.start()
            print "::::: runned"
            running[proc_name] = proc
    run_processes()
    while True:
        if len(running) == 0:
            return
        msg = globio.get()
        print msg
        if msg[0] == 'error' or msg[0] == 'finished':
            print "::::::::delete " + msg[1]
            proc_names.append(msg[1])
            running.pop(msg[1])
            run_processes()
            print "::::::::done"

class TestSuite(object):
    def __init__(self, suite_path, server):
        self.tests = []
        self.args = MockArgs()
        self.ini = {'server':server}
        self.suite_path = suite_path

        assert(os.path.isdir(self.suite_path))

        config = ConfigParser()
        config.read(os.path.join(self.suite_path, 'suite.ini'))
        self.ini.update(dict(config.items('default')))

        for i in ["config", "init_lua"]:
            self.ini[i] = os.path.join(suite_path, self.ini[i]) if i in self.ini else None
        for i in ["disabled", "valgrind_disabled", "release_disabled"]:
            self.ini[i] = dict.fromkeys(self.ini[i].split()) if i in self.ini else dict()
        for i in ["lua_libs"]:
            self.ini[i] = map(lambda x: os.path.join(suite_path, x),
                    dict.fromkeys(self.ini[i].split()) if i in self.ini else
                    dict())

        self.ini['servers'] = {'default' : server}
        self.ini['connections'] = {'default' : [server.admin, 'default']}
        self.ini['vardir'] = self.args.vardir
        self.ini['builddir'] = self.args.builddir
        for i in self.ini['lua_libs']:
            shutil.copy(i, self.args.vardir)

        self.find_tests()

    def find_tests(self):
        self.tests = [LuaTest(k, self.args, self.ini) for k in sorted(glob.glob(os.path.join(self.suite_path, "*.test.lua")))]

    def run_all(self, server, queue, num):
        print "start", num
        self.queue = queue
        self.server = server
        self.queue.put(('started', num))
        if not self.tests:
            return 0
        try:
            for test in self.tests:
                print "running ", test.name
                a = test.run(self.server)
                #self.queue.put(a)
                if not test.passed():
                    self.queue.put(('failed', num, test.name, a))
                else:
                    self.queue.put(('passed', num, test.name, a))
        except Exception as e:
            self.queue.put(('error', num, e))
            return 1
            #raise
        print "finished"
        self.queue.put(('finished', num))


server = Server('tarantool')
server.deploy('paral/tarantool.cfg',
              server.find_exe(MockArgs.builddir, silent=False),
              MockArgs.vardir, False, False,
              False, False,
              False, silent=False)
suits = search_suits(server)

run_tests(suits, server)


server.stop(silent=False)
server.cleanup()

#class Test:
#    """An individual test file. A test object can run itself
#    and remembers completion state of the run.
#
#    If file <test_name>.skipcond is exists it will be executed before
#    test and if it sets self.skip to True value the test will be skipped.
#    """
#
#    def __init__(self, name, args, suite_ini):
#        """Initialize test properties: path to test file, path to
#        temporary result file, path to the client program, test status."""
#        rg = re.compile('.test.*')
#        self.name = name
#        self.args = args
#        self.suite_ini = suite_ini
#        self.result = rg.sub('.result', name)
#        self.skip_cond = rg.sub('.skipcond', name)
#        self.tmp_result = os.path.join(self.args.vardir,
#                                       os.path.basename(self.result))
#        self.reject = "{0}/test/{1}".format(self.args.builddir,
#                                            rg.sub('.reject', name))
#        self.is_executed = False
#        self.is_executed_ok = None
#        self.is_equal_result = None
#        self.is_valgrind_clean = True
#
#    def passed(self):
#        """Return true if this test was run successfully."""
#        return self.is_executed and self.is_executed_ok and self.is_equal_result
#
#    def execute(self):
#        pass
#
#    def run(self, server):
#        """Execute the test assuming it's a python program.
#        If the test aborts, print its output to stdout, and raise
#        an exception. Else, comprare result and reject files.
#        If there is a difference, print it to stdout and raise an
#        exception. The exception is raised only if is_force flag is
#        not set."""
#        diagnostics = "unknown"
#        save_stdout = sys.stdout
#        try:
#            self.skip = False
#            if os.path.exists(self.skip_cond):
#                sys.stdout = FilteredStream(self.tmp_result)
#                stdout_fileno = sys.stdout.stream.fileno()
#                execfile(self.skip_cond, dict(locals(), **server.__dict__))
#                sys.stdout.close()
#                sys.stdout = save_stdout
#            if not self.skip:
#                sys.stdout = FilteredStream(self.tmp_result)
#                stdout_fileno = sys.stdout.stream.fileno()
#                self.execute(server)
#                sys.stdout.stream.flush()
#            self.is_executed_ok = True
#        except Exception as e:
#            traceback.print_exc(e)
#            diagnostics = str(e)
#        finally:
#            if sys.stdout and sys.stdout != save_stdout:
#                sys.stdout.close()
#            sys.stdout = save_stdout
#        self.is_executed = True
#        sys.stdout.flush()
#
#        if not self.skip:
#            if self.is_executed_ok and os.path.isfile(self.result):
#                self.is_equal_result = filecmp.cmp(self.result, self.tmp_result)
#        else:
#            self.is_equal_result = 1
#
#        if self.skip:
#            if os.path.exists(self.tmp_result):
#                os.remove(self.tmp_result)
#            return "skipped"
#        elif self.is_executed_ok and self.is_equal_result and self.is_valgrind_clean:
#            if os.path.exists(self.tmp_result):
#                os.remove(self.tmp_result)
#            return "passed"
#        elif (self.is_executed_ok and not self.is_equal_result and not
#              os.path.isfile(self.result)):
#            os.rename(self.tmp_result, self.result)
#            return "new"
#        else:
#            os.rename(self.tmp_result, self.reject)
#
#            where = ""
#            if not self.is_executed_ok:
#                part = self.return_diagnostics(self.reject, "Test failed! Last 10 lines of the result file:")
#                where = ": test execution aborted, reason '{0}'".format(diagnostics)
#            elif not self.is_equal_result:
#                part = self.return_unidiff()
#                where = ": wrong test output"
#            elif not self.is_valgrind_clean:
#                os.remove(self.reject)
#                part = self.return_diagnostics(server.valgrind_log, "Test failed! Last 10 lines of valgrind.log:")
#                where = ": there were warnings in valgrind.log"
#            return ("failed", *part, where)
#
#    def return_diagnostics(self, logfile, message):
#        return (message, format_tail_n(logfile, 10))
#
#    def return_unidiff(self):
#        with open(self.result, "r") as result, open(self.reject, "r") as reject:
#            result_time = time.ctime(os.stat(self.result).st_mtime)
#            reject_time = time.ctime(os.stat(self.reject).st_mtime)
#            diff = difflib.unified_diff(result.readlines(),
#                                        reject.readlines(),
#                                        self.result,
#                                        self.reject,
#                                        result_time,
#                                        reject_time)
#            return diff
#
